#pragma once

typedef enum {
    EVNET_NONE = 0x0,
    EVENT_READABLE = 0x1,
    EVENT_WRITABLE = 0x2,
} event_t;

typedef void (*event_proc_t)(void *args);

int add_fd_event(int fd, event_t what, event_proc_t proc, void *args);
void del_fd_event(int fd, event_t what);

void event_cycle(int milliseconds);
void event_loop_init(int max_conn);
