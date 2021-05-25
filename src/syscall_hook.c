#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include "coroutine/sched.h"
#include "event.h"
#include "internal.h"
#include "util/net.h"

typedef int (*sys_connect)(int sockfd,
                           const struct sockaddr *addr,
                           socklen_t addrlen);
typedef int (*sys_accept)(int sockfd,
                          struct sockaddr *addr,
                          socklen_t *addrlen);

typedef ssize_t (*sys_read)(int fd, void *buf, size_t count);
typedef ssize_t (*sys_recv)(int sockfd, void *buf, size_t len, int flags);

typedef ssize_t (*sys_write)(int fd, const void *buf, size_t count);
typedef ssize_t (*sys_send)(int sockfd, const void *buf, size_t len, int flags);

#define HOOK_SYSCALL(name) \
    real_sys_##name = (sys_##name) dlsym(RTLD_NEXT, #name)

#define CONN_TIMEOUT (10 * 1000)
#define ACCEPT_TIMEOUT (3 * 1000)

#define READ_TIMEOUT (10 * 1000)
#define RECV_TIMEOUT (10 * 1000)

#define WRITE_TIMEOUT (10 * 1000)
#define SEND_TIMEOUT (10 * 1000)

#define KEEP_ALIVE 60

static sys_connect real_sys_connect = NULL;
static sys_accept real_sys_accept = NULL;

static sys_read real_sys_read = NULL;
static sys_recv real_sys_recv = NULL;

sys_write real_sys_write = NULL; /* shared with log.c */
static sys_send real_sys_send = NULL;

#define fd_not_ready() ((EAGAIN == errno) || (EWOULDBLOCK == errno))

static void event_rw_callback(void *args)
{
    wakeup_coro(args);
}

static void event_conn_callback(void *args)
{
    wakeup_coro_priority(args);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    set_nonblock(sockfd);

    int ret = real_sys_connect(sockfd, addr, addrlen);
    if (0 == ret) /* successful */
        return 0;

    if (ret < 0 && errno != EINPROGRESS)
        return -1;

    if (add_fd_event(sockfd, EVENT_WRITABLE, event_conn_callback,
                     current_coro()))
        return -2;

    schedule_timeout(CONN_TIMEOUT);
    del_fd_event(sockfd, EVENT_WRITABLE);
    if (is_wakeup_by_timeout()) {
        errno = ETIMEDOUT;
        return -3;
    }

    int flags;
    socklen_t len;
    ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &flags, &len);
    if (ret == -1 || flags || !len) {
        if (flags)
            errno = flags;

        return -4;
    }

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int connfd = 0;

    while ((connfd = real_sys_accept(sockfd, addr, addrlen)) < 0) {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_READABLE, event_conn_callback,
                         current_coro()))
            return -2;

        schedule_timeout(ACCEPT_TIMEOUT);
        del_fd_event(sockfd, EVENT_READABLE);
        if (is_wakeup_by_timeout()) {
            errno = ETIME;
            return -3;
        }
    }

    if (set_nonblock(connfd)) {
        close(connfd);
        return -4;
    }

    if (enable_tcp_no_delay(connfd)) {
        close(connfd);
        return -5;
    }

    if (set_keep_alive(connfd, KEEP_ALIVE)) {
        close(connfd);
        return -6;
    }

    return connfd;
}

ssize_t read(int fd, void *buf, size_t count)
{
    ssize_t n;

    while ((n = real_sys_read(fd, buf, count)) < 0) {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(fd, EVENT_READABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(READ_TIMEOUT);
        del_fd_event(fd, EVENT_READABLE);
        if (is_wakeup_by_timeout()) {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    ssize_t n;

    while ((n = real_sys_recv(sockfd, buf, len, flags)) < 0) {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_READABLE, event_rw_callback,
                         current_coro()))
            return -2;

        schedule_timeout(RECV_TIMEOUT);
        del_fd_event(sockfd, EVENT_READABLE);
        if (is_wakeup_by_timeout()) {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    ssize_t n;

    while ((n = real_sys_write(fd, buf, count)) < 0) {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(fd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(WRITE_TIMEOUT);
        del_fd_event(fd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout()) {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    ssize_t n;

    while ((n = real_sys_send(sockfd, buf, len, flags)) < 0) {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_WRITABLE, event_rw_callback,
                         current_coro()))
            return -2;

        schedule_timeout(SEND_TIMEOUT);
        del_fd_event(sockfd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout()) {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

__INIT static void syscall_hook_init()
{
    HOOK_SYSCALL(connect);
    HOOK_SYSCALL(accept);

    HOOK_SYSCALL(read);
    HOOK_SYSCALL(recv);

    HOOK_SYSCALL(write);
    HOOK_SYSCALL(send);
}
