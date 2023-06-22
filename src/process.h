#pragma once

#include <signal.h>

enum {
    SHUTDOWN_SIGNAL = SIGQUIT,
    TERMINATE_SIGNAL = SIGINT,
    RECONFIGURE_SIGNAL = SIGHUP
};

enum proc_type { MASTER_PROCESS = 0, WORKER_PROCESS = 1 };
extern enum proc_type g_process_type;

extern int g_shall_stop, g_shall_exit;

typedef int (*master_init_proc_t)();
typedef int (*worker_init_proc_t)();
typedef void (*request_handler_t)(int fd);

void register_service(master_init_proc_t master_proc,
                      worker_init_proc_t worker_proc,
                      request_handler_t handler);

void worker_process_cycle();
void master_process_cycle();
void worker_exit_handler(int pid);

void tcp_srv_init();
void process_init();
