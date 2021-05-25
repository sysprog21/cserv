#ifndef UTIL_SPINLOCK_H
#define UTIL_SPINLOCK_H

/* Ticket spinlock implementation */

#include <sched.h>
#include <stdatomic.h>

typedef struct {
    volatile unsigned int start;
    char padding[64]; /* avoid false sharing between start and ticket */
    _Atomic unsigned int ticket;
} spinlock_t;

static inline void spin_lock_init(spinlock_t *lock)
{
    lock->start = 0;
    atomic_init(&lock->ticket, 0);
}

static inline int spin_trylock(spinlock_t *lock)
{
    /* See which ticket is being served */
    unsigned int ticket = lock->start;

    /* Take a ticket, but only if it is being served already */
    return atomic_compare_exchange_strong_explicit(
        &lock->ticket, &ticket, ticket + 1U, memory_order_seq_cst,
        memory_order_relaxed);
}

static inline void spin_lock(spinlock_t *lock)
{
    unsigned int ticket =
        atomic_fetch_add_explicit(&lock->ticket, 1, memory_order_relaxed);

    /* Wait until our ticket is being served */
    while (lock->start != ticket)
        sched_yield();

    atomic_thread_fence(memory_order_seq_cst);
}

static inline void spin_unlock(spinlock_t *lock)
{
    atomic_thread_fence(memory_order_seq_cst);
    lock->start++;
}

#endif
