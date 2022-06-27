#ifndef CORE_SYSCALL_HOOK_H
#define CORE_SYSCALL_HOOK_H

#include <sys/socket.h>

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

/* declared in syscall_hook.c */
extern sys_write real_sys_write;

#endif