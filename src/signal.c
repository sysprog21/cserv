#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "internal.h"
#include "logger.h"
#include "process.h"

struct sys_signal {
    int signo;
    char *name;
    void (*handler)(int signo);
};

static void worker_process_get_status()
{
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        WARN("worker process %d exited", pid);
        log_worker_flush_and_reset(pid);
        worker_exit_handler(pid);
    }
}

static void master_signal_handler(int signo)
{
    switch (signo) {
    case SIGQUIT:
        g_shall_stop = 1;
        break;
    case SIGTERM:
    case SIGINT:
        g_shall_exit = 1;
        break;

    case SIGCHLD:
        worker_process_get_status();
        break;
    }
}

static void worker_signal_handler(int signo)
{
    switch (signo) {
    case SIGQUIT:
        g_shall_stop = 1;
        INFO("In worker, received SIGQUIT");
        break;
    case SIGTERM:
    case SIGINT:
        g_shall_exit = 1;
        INFO("In worker, received SIGTERM/SIGINT");
        break;
    }
}

static void signal_handler(int signo);

static struct sys_signal signals[] = {{SIGQUIT, "stop", signal_handler},
                                      {SIGTERM, "force-quit", signal_handler},
                                      {SIGINT, "force-quit", signal_handler},
                                      {SIGHUP, "reload", signal_handler},
                                      {SIGCHLD, "default", signal_handler},
                                      {SIGSYS, "ignored", SIG_IGN},
                                      {SIGPIPE, "ignored", SIG_IGN},
                                      {0, "", NULL}};

static void signal_handler(int signo)
{
    for (struct sys_signal *sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo)
            break;
    }

    switch (g_process_type) {
    case MASTER_PROCESS:
        master_signal_handler(signo);
        break;

    case WORKER_PROCESS:
        worker_signal_handler(signo);
        break;

    default: /* should not reach */
        break;
    }
}

__INIT static void signal_init()
{
    struct sigaction sa;

    for (struct sys_signal *sig = signals; sig->signo; sig++) {
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            printf("Failed to invoke sigaction(%d)\n", sig->signo);
            exit(0);
        }
    }
}
