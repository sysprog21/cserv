#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "event.h"

struct fd_event {
    int mask;
    event_proc_t proc;
    void *args;
};

struct event_loop {
    int max_conn; /* max fd allowed */
    struct fd_event *array;
    int epoll_fd;
    struct epoll_event *ev;
};

static struct event_loop evloop;

int add_fd_event(int fd, event_t what, event_proc_t proc, void *args)
{
    if (fd >= evloop.max_conn) {
        errno = ERANGE;
        return -1;
    }

    struct fd_event *fe = &evloop.array[fd];
    int op = (EVNET_NONE == fe->mask) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    struct epoll_event ev = {.events = 0, .data.fd = fd};
    int mask = what | fe->mask; /* merge old event */

    if (mask & EVENT_READABLE)
        ev.events |= EPOLLIN;
    if (mask & EVENT_WRITABLE)
        ev.events |= EPOLLOUT;

    if (-1 == epoll_ctl(evloop.epoll_fd, op, fd, &ev))
        return -2;

    fe->mask |= what;
    fe->proc = proc;
    fe->args = args;

    return 0;
}

void del_fd_event(int fd, event_t what)
{
    if (fd >= evloop.max_conn)
        return;

    struct fd_event *fe = &evloop.array[fd];
    if (EVNET_NONE == fe->mask)
        return;

    fe->mask &= ~(what);
    int mask = fe->mask;
    struct epoll_event ev = {.events = 0, .data.fd = fd};

    if (mask & EVENT_READABLE)
        ev.events |= EPOLLIN;
    if (mask & EVENT_WRITABLE)
        ev.events |= EPOLLOUT;

    epoll_ctl(evloop.epoll_fd,
              (mask == EVNET_NONE) ? EPOLL_CTL_DEL : EPOLL_CTL_MOD, fd, &ev);
}

void event_cycle(int ms /* in milliseconds */)
{
    int value = epoll_wait(evloop.epoll_fd, evloop.ev, evloop.max_conn, ms);
    if (value <= 0)
        return;

    for (int i = 0; i < value; i++) {
        struct epoll_event *ev = &evloop.ev[i];
        struct fd_event *fe = &evloop.array[ev->data.fd];
        fe->proc(fe->args);
    }
}

void event_loop_init(int max_conn)
{
    evloop.max_conn = max_conn + 128; /* reserved for the system */

    evloop.array = calloc(evloop.max_conn, sizeof(struct fd_event));
    if (!evloop.array) {
        printf("Failed to allocate fd evloop\n");
        exit(0);
    }

    evloop.epoll_fd = epoll_create(1024);
    if (-1 == evloop.epoll_fd) {
        printf("Failed to create epoll: %s\n", strerror(errno));
        exit(0);
    }

    evloop.ev = malloc(evloop.max_conn * sizeof(struct epoll_event));
    if (!evloop.ev) {
        printf("Failed to allocate epoll evloop\n");
        exit(0);
    }
}
