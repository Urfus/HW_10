#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/string.h>

#include "../inc/kernel_msgpool.h"
#include "../inc/alloc.h"
#include "../inc/queue.h"
#include "../inc/params.h"

struct msgpool_ctx *g_ctx = NULL;
static void consumer_timer_callback(struct timer_list *t)
{
    struct msgpool_ctx *ctx = container_of(t, struct msgpool_ctx, consumer_timer);
    struct msg *m;
    ktime_t now;
    s64 elapsed_ns;
    unsigned long flags;
    now = ktime_get();

    while ((m = queue_dequeue(&ctx->queue)) != NULL) {
        elapsed_ns = ktime_to_ns(ktime_sub(now, m->enqueue_time));
        
        pr_info("msgpool: [%u] %s (queued %lld ns ago)\n",
                m->seq, m->text, elapsed_ns);
        
        spin_lock_irqsave(&ctx->last_msg_lock, flags);
        strscpy(ctx->last_msg, m->text, MSG_TEXT_MAX);
        spin_unlock_irqrestore(&ctx->last_msg_lock, flags);
        
        msg_free(ctx, m);
        atomic_inc(&ctx->consumed_total);
    }

    mod_timer(&ctx->consumer_timer, 
            jiffies + msecs_to_jiffies(ctx->interval_ms));

}
static int __init kernel_msgpool(void)
{
    pr_info("Kernel msgpool: module load startes ...\n");

    int ret;
    g_ctx = kzalloc(sizeof(struct msgpool_ctx), GFP_KERNEL);
    if (!g_ctx)
        return -ENOMEM;

    queue_init(&g_ctx->queue);
    spin_lock_init(&g_ctx->last_msg_lock);

    atomic_set(&g_ctx->sent_total, 0);
    atomic_set(&g_ctx->consumed_total, 0);
    atomic_set(&g_ctx->flushed_total, 0);
    atomic_set(&g_ctx->dropped_total, 0);

    g_ctx->seq_counter = 0;
    strscpy(g_ctx->last_msg, "(empty)", MSG_TEXT_MAX);

    ret = params_init(g_ctx);
    if (ret) {
        kfree(g_ctx);
        return ret;
    }

    ret = alloc_init(g_ctx, g_ctx->alloc_type, g_ctx->pool_min_nr);
    if (ret) {
        params_cleanup();
        kfree(g_ctx);
        return ret;
    }

    timer_setup(&g_ctx->consumer_timer, consumer_timer_callback, 0);
    mod_timer(&g_ctx->consumer_timer, 
            jiffies + msecs_to_jiffies(g_ctx->interval_ms));

    pr_info("Kernel msgpool: module loaded (alloc_type=%u, interval_ms=%u)\n",
            g_ctx->alloc_type, g_ctx->interval_ms);

    return 0;
}

static void __exit kernel_msgpool_exit(void)
{
    pr_info("Kernel msgpool: module unload started ...\n");
    timer_delete_sync(&g_ctx->consumer_timer);

    queue_flush(&g_ctx->queue, NULL);

    params_cleanup();
    alloc_cleanup(g_ctx);

    kfree(g_ctx);

    pr_info("Kernel msgpool: module unloaded\n");
}

module_init(kernel_msgpool);
module_exit(kernel_msgpool_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fedor Kazakov");
MODULE_DESCRIPTION("Otus home work 10 (Message pool module with kmem_cache and mempool)");
MODULE_VERSION("1.0");

