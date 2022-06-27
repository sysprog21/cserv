#ifndef CORE_SYSCALL_HOOK_H
#define CORE_SYSCALL_HOOK_H

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

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

extern sys_connect real_sys_connect;
extern sys_accept real_sys_accept;

extern sys_read real_sys_read;
extern sys_recv real_sys_recv;

extern sys_write real_sys_write;
extern sys_send real_sys_send;

#define fd_not_ready() ((EAGAIN == errno) || (EWOULDBLOCK == errno))

/* hooked system calls */
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t read(int fd, void *buf, size_t count);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t write(int fd, const void *buf, size_t count);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);

#endif