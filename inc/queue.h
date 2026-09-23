#ifndef QUEUE_H
#define QUEUE_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>

#define MSG_TEXT_MAX  128
#define MSG_QUEUE_MAX 16

struct msg {
    char          text[MSG_TEXT_MAX];
    ktime_t       enqueue_time;
    unsigned int  seq;
};

struct msg_queue {
    struct msg   *slots[MSG_QUEUE_MAX];
    unsigned int  head;
    unsigned int  tail;
    unsigned int  count;
    spinlock_t    lock;
};

void queue_init(struct msg_queue *q);
int queue_enqueue(struct msg_queue *q, struct msg *m);
struct msg *queue_dequeue(struct msg_queue *q);
void queue_flush(struct msg_queue *q, void (*free_fn)(struct msg *));
unsigned int queue_count(struct msg_queue *q);

enum hrtimer_restart producer_timer_fn(struct hrtimer *timer);

#endif // QUEUE_H
