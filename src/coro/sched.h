#ifndef CORO_SCHED_H
#define CORO_SCHED_H

#include <stdbool.h>

typedef void (*coro_func)(void *args);

void schedule_cycle();
int dispatch_coro(coro_func func, void *args);
void schedule_timeout(int milliseconds);
bool is_wakeup_by_timeout();

void wakeup_coro(void *args);
void wakeup_coro_priority(void *args);
void *current_coro();

void schedule_init(size_t stack_kbytes, size_t max_coro_size);

#endif
