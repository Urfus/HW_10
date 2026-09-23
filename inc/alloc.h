#ifndef ALLOC_H
#define ALLOC_H

#include <linux/slab.h>
#include <linux/mempool.h>

#include "../inc/queue.h"

struct msgpool_ctx;
int alloc_init(struct msgpool_ctx *ctx, unsigned int alloc_type, unsigned int pool_min_nr);
void alloc_cleanup(struct msgpool_ctx *ctx);
struct msg *msg_alloc(struct msgpool_ctx *ctx);
void msg_free(struct msgpool_ctx *ctx, struct msg *m);

#endif // ALLOC_H