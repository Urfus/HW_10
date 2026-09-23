#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>

#include "../inc/queue.h"

void queue_init(struct msg_queue *q)
{
    unsigned int i;
    for (i = 0; i < MSG_QUEUE_MAX; i++)
    q->slots[i] = NULL;

    q->head = 0;
    q->tail = 0;
    q->count = 0;
    spin_lock_init(&q->lock);
}

int queue_enqueue(struct msg_queue *q, struct msg *m)
{
    unsigned long flags;
    spin_lock_irqsave(&q->lock, flags);

    if (q->count >= MSG_QUEUE_MAX) {
        spin_unlock_irqrestore(&q->lock, flags);
        return -ENOBUFS;
    }

    q->slots[q->tail] = m;
    q->tail = (q->tail + 1) % MSG_QUEUE_MAX;
    q->count++;

    spin_unlock_irqrestore(&q->lock, flags);
    return 0;
}

struct msg *queue_dequeue(struct msg_queue *q)
{
    struct msg *m;
    unsigned long flags;
    spin_lock_irqsave(&q->lock, flags);

    if (q->count == 0) {
        spin_unlock_irqrestore(&q->lock, flags);
        return NULL;
    }

    m = q->slots[q->head];
    q->slots[q->head] = NULL;
    q->head = (q->head + 1) % MSG_QUEUE_MAX;
    q->count--;

    spin_unlock_irqrestore(&q->lock, flags);
    return m;
}

void queue_flush(struct msg_queue *q, void (*free_fn)(struct msg *))
{
    struct msg *m;
    while ((m = queue_dequeue(q)) != NULL) {
    if (free_fn)
        free_fn(m);
    }
}

unsigned int queue_count(struct msg_queue *q)
{
    unsigned int count;
    unsigned long flags;
    spin_lock_irqsave(&q->lock, flags);
    count = q->count;
    spin_unlock_irqrestore(&q->lock, flags);

    return count;
}
