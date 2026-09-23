#ifndef KERNEL_MSGPOOL_H
#define KERNEL_MSGPOOL_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include "queue.h"
#include <linux/slab.h>
#include <linux/mempool.h>

struct msgpool_ctx {
    unsigned int      alloc_type;
    unsigned int      pool_min_nr;
    unsigned int      interval_ms;

    struct kmem_cache *msg_cache; /* slab-кеш объектов struct msg       */
    mempool_t         *msg_pool;  /* пул поверх msg_cache (alloc_type=1) */

    struct msg_queue  queue;

    struct timer_list consumer_timer;

    atomic_t  sent_total;       /* всего поставлено в очередь         */
    atomic_t  consumed_total;   /* всего обработано таймером          */
    atomic_t  flushed_total;    /* всего сброшено через flush         */
    atomic_t  dropped_total;    /* отброшено (очередь была полна)     */

    /* Последнее обработанное сообщение (для параметра inbox) */
    char      last_msg[MSG_TEXT_MAX];

    spinlock_t last_msg_lock;

    unsigned int seq_counter;   /* монотонный счётчик сообщений       */
};

extern struct msgpool_ctx *g_ctx;

#endif // KERNEL_MSGPOOL_H
