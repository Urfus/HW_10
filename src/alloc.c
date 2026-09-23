#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mempool.h>

#include "../inc/alloc.h"
#include "../inc/kernel_msgpool.h"

int alloc_init(struct msgpool_ctx *ctx, unsigned int alloc_type, unsigned int pool_min_nr)
{
    ctx->alloc_type = alloc_type;
    ctx->pool_min_nr = pool_min_nr;
    ctx->msg_cache = kmem_cache_create("msgpool_cache", sizeof(struct msg),
                                        0, SLAB_HWCACHE_ALIGN, NULL);
    if (!ctx->msg_cache) {
        pr_err("msgpool: failed to create kmem_cache\n");
        return -ENOMEM;
    }

    if (alloc_type == 1) {
        ctx->msg_pool = mempool_create_slab_pool(pool_min_nr, ctx->msg_cache);
        if (!ctx->msg_pool) {
            pr_err("msgpool: failed to create mempool\n");
            kmem_cache_destroy(ctx->msg_cache);
            ctx->msg_cache = NULL;
            return -ENOMEM;
        }
    } else {
        ctx->msg_pool = NULL;
    }

    return 0;
}
void alloc_cleanup(struct msgpool_ctx *ctx)
{
    if (ctx->msg_pool) {
        mempool_destroy(ctx->msg_pool);
        ctx->msg_pool = NULL;
    }
    if (ctx->msg_cache) {
        kmem_cache_destroy(ctx->msg_cache);
        ctx->msg_cache = NULL;
    }
}

struct msg *msg_alloc(struct msgpool_ctx *ctx)
{
    struct msg *m;

    if (ctx->alloc_type == 1) {
        m = mempool_alloc(ctx->msg_pool, GFP_KERNEL);
    } else {
        m = kmem_cache_alloc(ctx->msg_cache, GFP_KERNEL);
    }

    return m;
}

void msg_free(struct msgpool_ctx *ctx, struct msg *m)
{
    if (!m)
        return;
    if (ctx->alloc_type == 1) {
        mempool_free(m, ctx->msg_pool);
    } else {
        kmem_cache_free(ctx->msg_cache, m);
    }
}