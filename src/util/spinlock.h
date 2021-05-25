#ifndef UTIL_SPINLOCK_H
#define UTIL_SPINLOCK_H

#include <sched.h>

typedef struct {
    volatile int counter;
} spinlock_t;

#if defined(__x86_64__) || defined(__i386__)
#define __cpu_yield() __asm__ volatile("pause")
#else
#define __cpu_yield()
#endif

static inline void spin_lock_init(spinlock_t *lock)
{
    lock->counter = 0;
}

static inline int spin_trylock(spinlock_t *lock)
{
    return (lock->counter == 0 &&
            __sync_bool_compare_and_swap(&lock->counter, 0, -1));
}

static inline void spin_lock(spinlock_t *lock)
{
    for (;;) {
        if (lock->counter == 0 &&
            __sync_bool_compare_and_swap(&lock->counter, 0, -1))
            return;

#ifdef CONFIG_SMP
        for (int n = 1; n < 2048; n <<= 1) {
            for (int i = 0; i < n; i++)
                __cpu_yield();

            if (lock->counter == 0 &&
                __sync_bool_compare_and_swap(&lock->counter, 0, -1))
                return;
        }
#endif

        sched_yield();
    }
}

static inline void spin_unlock(spinlock_t *lock)
{
    __sync_bool_compare_and_swap(&lock->counter, -1, 0);
}

#endif
