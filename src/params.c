#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "../inc/params.h"
#include "../inc/alloc.h"
#include "../inc/queue.h"
#include "../inc/kernel_msgpool.h"

static struct msgpool_ctx *param_ctx = NULL;
static unsigned int alloc_type_val = 0;
static unsigned int pool_min_nr_val = 8;
static unsigned int interval_ms_val = 1000;

// ==== alloc_type ===
static int param_set_alloc_type(const char *val, const struct kernel_param *kp)
{
    unsigned int new_val;
    int ret;
    ret = kstrtouint(val, 0, &new_val);
    if (ret)
        return -EINVAL;

    if (new_val > 1)
        return -EINVAL;

    if (param_ctx && queue_count(&param_ctx->queue) > 0)
        return -EBUSY;

    if (param_ctx && param_ctx->alloc_type != new_val) {
        alloc_cleanup(param_ctx);
        ret = alloc_init(param_ctx, new_val, param_ctx->pool_min_nr);
        if (ret)
            return ret;
    }

    alloc_type_val = new_val;
    return 0;
}
static int param_get_alloc_type(char *buffer, const struct kernel_param *kp)
{
    return sprintf(buffer, "%u\n", alloc_type_val);
}
static const struct kernel_param_ops alloc_type_ops = {
    .set = param_set_alloc_type,
    .get = param_get_alloc_type,
};
module_param_cb(alloc_type, &alloc_type_ops, &alloc_type_val, 0644);

// === pool_min_nr ===
static int param_set_pool_min_nr(const char *val, const struct kernel_param *kp)
{
    unsigned int new_val;
    int ret;
    ret = kstrtouint(val, 0, &new_val);
    if (ret)
        return -EINVAL;

    if (new_val < 1 || new_val > 64)
        return -EINVAL;

    if (param_ctx && queue_count(&param_ctx->queue) > 0)
        return -EBUSY;

    pool_min_nr_val = new_val;
    return 0;
}

static int param_get_pool_min_nr(char *buffer, const struct kernel_param *kp)
{
    return sprintf(buffer, "%u\n", pool_min_nr_val);
}
static const struct kernel_param_ops pool_min_nr_ops = {
    .set = param_set_pool_min_nr,
    .get = param_get_pool_min_nr,
};
module_param_cb(pool_min_nr, &pool_min_nr_ops, &pool_min_nr_val, 0644);

// === interval_ms ===
static int param_set_interval_ms(const char *val, const struct kernel_param *kp)
{
    unsigned int new_val;
    int ret;
    ret = kstrtouint(val, 0, &new_val);
    if (ret)
        return -EINVAL;

    if (new_val < 100 || new_val > 60000)
        return -EINVAL;

    interval_ms_val = new_val;

    if (param_ctx) {
        param_ctx->interval_ms = new_val;
        mod_timer(&param_ctx->consumer_timer,
                jiffies + msecs_to_jiffies(new_val));
    }

    return 0;
}

static int param_get_interval_ms(char *buffer, const struct kernel_param *kp)
{
    return sprintf(buffer, "%u\n", interval_ms_val);
}
static const struct kernel_param_ops interval_ms_ops = {
    .set = param_set_interval_ms,
    .get = param_get_interval_ms,
};
module_param_cb(interval_ms, &interval_ms_ops, &interval_ms_val, 0644);

// === send ===
static int param_set_send(const char *val, const struct kernel_param *kp)
{
    struct msg *m;
    size_t len;
    int ret;
    if (!param_ctx)
        return -EINVAL;

    len = strlen(val);
    if (len == 0)
        return -EINVAL;

    m = msg_alloc(param_ctx);
    if (!m)
        return -ENOMEM;

    if (len >= MSG_TEXT_MAX) {
        pr_warn("msgpool: message too long, truncating\n");
        len = MSG_TEXT_MAX - 1;
    }

    strscpy(m->text, val, MSG_TEXT_MAX);
    m->enqueue_time = ktime_get();
    m->seq = param_ctx->seq_counter++;

    ret = queue_enqueue(&param_ctx->queue, m);
    if (ret) {
        msg_free(param_ctx, m);
        atomic_inc(&param_ctx->dropped_total);
        return -ENOBUFS;
    }

    atomic_inc(&param_ctx->sent_total);
    return 0;
}

static int param_get_send(char *buffer, const struct kernel_param *kp)
{
    return sprintf(buffer, "(write-only)\n");
}
static const struct kernel_param_ops send_ops = {
    .set = param_set_send,
    .get = param_get_send,
};
module_param_cb(send, &send_ops, NULL, 0200);

// === inbox ===
static int param_get_inbox(char *buffer, const struct kernel_param *kp)
{
    unsigned long flags;
    if (!param_ctx)
        return sprintf(buffer, "(empty)\n");

    spin_lock_irqsave(&param_ctx->last_msg_lock, flags);
    snprintf(buffer, PAGE_SIZE, "%s\n", param_ctx->last_msg);
    spin_unlock_irqrestore(&param_ctx->last_msg_lock, flags);

    return strlen(buffer);
}

static const struct kernel_param_ops inbox_ops = {
    .get = param_get_inbox,
};
module_param_cb(inbox, &inbox_ops, NULL, 0444);

// === stats ===
static int param_get_stats(char *buffer, const struct kernel_param *kp)
{
    unsigned int queued;
    const char *alloc_str;

    if (!param_ctx)
        return sprintf(buffer, "not initialized\n");

    queued = queue_count(&param_ctx->queue);
    alloc_str = (param_ctx->alloc_type == 0) ? "kmem_cache" : "mempool";

    return sprintf(buffer, "sent=%u consumed=%u flushed=%u dropped=%u queued=%u alloc=%s interval_ms=%u\n",
                atomic_read(&param_ctx->sent_total),
                atomic_read(&param_ctx->consumed_total),
                atomic_read(&param_ctx->flushed_total),
                atomic_read(&param_ctx->dropped_total),
                queued,
                alloc_str,
                param_ctx->interval_ms);
}
static const struct kernel_param_ops stats_ops = {
    .get = param_get_stats,
};
module_param_cb(stats, &stats_ops, NULL, 0444);

// === flush ===
static int param_set_flush(const char *val, const struct kernel_param *kp)
{
    unsigned int val_int;
    struct msg *m;
    unsigned int flushed = 0;
    int ret;
    ret = kstrtouint(val, 0, &val_int);
    if (ret)
        return -EINVAL;

    if (val_int == 0)
        return 0;

    if (!param_ctx)
        return -EINVAL;

    while ((m = queue_dequeue(&param_ctx->queue)) != NULL) {
        msg_free(param_ctx, m);
        flushed++;
    }

    if (flushed > 0) {
        atomic_add(flushed, &param_ctx->flushed_total);
        pr_info("msgpool: flushed %u message(s)\n", flushed);
    }

    return 0;
}

static int param_get_flush(char *buffer, const struct kernel_param *kp)
{
    return sprintf(buffer, "(write-only)\n");
}
static const struct kernel_param_ops flush_ops = {
    .set = param_set_flush,
    .get = param_get_flush,
};

module_param_cb(flush, &flush_ops, NULL, 0200);


int params_init(struct msgpool_ctx *ctx)
{
    param_ctx = ctx;
    ctx->alloc_type = alloc_type_val;
    ctx->pool_min_nr = pool_min_nr_val;
    ctx->interval_ms = interval_ms_val;

    return 0;
}

void params_cleanup(void)
{
    param_ctx = NULL;
}