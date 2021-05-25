#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "coro/sched.h"
#include "env.h"
#include "event.h"
#include "internal.h"
#include "logger.h"
#include "process.h"
#include "util/net.h"
#include "util/shm.h"
#include "util/spinlock.h"
#include "util/system.h"

#define INVALID_PID -1 /* non-existing process ID */

struct process {
    pid_t pid;
    int cpuid; /* bounded cpu ID */
};

static inline void request_default_handler(int fd __UNUSED) {}

static master_init_proc_t master_init_proc = NULL;
static worker_init_proc_t worker_init_proc = NULL;
static request_handler_t request_handler = request_default_handler;

static int listen_fd;
static spinlock_t *accept_lock; /* lock for accept fd */

static struct process worker[MAX_WORKER_PROCESS];
static int mastr_pid;
static int worker_pid; /* master pid included */
enum proc_type g_process_type;

static int connection_count = 1;      /* if 0, worker exits. */
static bool all_workers_exit = false; /* whether if all worker exist. */
static bool shall_create_worker = true;

int g_shall_stop = 0; /* shall we stop the service? if 0, continue */
int g_shall_exit = 0; /* shall we force quit the service? */

static int worker_empty()
{
    for (int i = 0; i < g_worker_processes; i++) {
        struct process *p = &worker[i];
        if (p->pid != INVALID_PID)
            return 0;
    }

    return 1;
}

static pid_t fork_worker(struct process *p)
{
    pid_t pid = fork();

    switch (pid) {
    case -1:
        ERR("Failed to fork worker process. master pid:%d", getpid());
        return -1;

    case 0:
        worker_pid = getpid();
        g_process_type = WORKER_PROCESS;
        set_proc_title("cserv: worker process");

        if (log_worker_alloc(worker_pid) < 0) {
            printf("Failed to alloc log for process:%d\n", worker_pid);
            exit(0);
        }

        if (bind_cpu(p->cpuid)) {
            ERR("Failed to bind cpu: %d\n", p->cpuid);
            exit(0);
        }

        return 0;

    default:
        p->pid = pid;
        return pid;
    }
}

static void spawn_worker_process()
{
    for (int i = 0; i < g_worker_processes; i++) {
        struct process *p = &worker[i];
        if (p->pid != INVALID_PID)
            continue;

        if (0 == fork_worker(p))
            break;
    }
}

static inline void increase_conn()
{
    connection_count++;
}

static inline void decrease_conn_and_check()
{
    if (--connection_count == 0)
        exit(0);
}

static void handle_connection(void *args)
{
    int connfd = (int) (intptr_t) args;

    request_handler(connfd);
    close(connfd);
    decrease_conn_and_check();
}

static inline int worker_can_accept()
{
    return connection_count < g_worker_connections;
}

static int worker_accept()
{
    struct sockaddr addr;
    socklen_t addrlen;
    int connfd;

    if (likely(g_worker_processes > 1)) {
        if (worker_can_accept() && spin_trylock(accept_lock)) {
            connfd = accept(listen_fd, &addr, &addrlen);
            spin_unlock(accept_lock);
            return connfd;
        }

        return 0;
    } else
        connfd = accept(listen_fd, &addr, &addrlen);

    return connfd;
}

static void worker_accept_cycle(void *args __UNUSED)
{
    for (;;) {
        if (unlikely(g_shall_stop)) {
            set_proc_title("cserv: worker process is shutting down");
            decrease_conn_and_check();
            break;
        }

        if (unlikely(g_shall_exit))
            exit(0);

        int connfd = worker_accept();
        if (likely(connfd > 0)) {
            if (dispatch_coro(handle_connection, (void *) (intptr_t) connfd)) {
                WARN("system busy to handle request.");
                close(connfd);
                continue;
            }
            increase_conn();
        } else if (connfd == 0) {
            schedule_timeout(200);
            continue;
        }
    }
}

void worker_process_cycle()
{
    if (worker_init_proc && worker_init_proc()) {
        ERR("Failed to init worker");
        exit(0);
    }

    schedule_init(g_coro_stack_kbytes, g_worker_connections);
    event_loop_init(g_worker_connections);
    dispatch_coro(worker_accept_cycle, NULL);
    INFO("worker success running....");
    schedule_cycle();
}

static void send_signal_to_workers(int signo)
{
    for (int i = 0; i < g_worker_processes; i++) {
        struct process *p = &worker[i];
        if (p->pid != INVALID_PID) {
            if (kill(p->pid, signo) == -1)
                ERR("Failed to send signal %d to child pid:%d", signo, p->pid);
        }
    }
}

void master_process_cycle()
{
    if (master_init_proc && master_init_proc()) {
        ERR("Failed to init master process, cserv exit");
        exit(0);
    }

    INFO("master success running....");

    for (;;) {
        if (g_shall_stop == 1) {
            WARN("notify worker processes to stop");
            send_signal_to_workers(SHUTDOWN_SIGNAL);
            g_shall_stop = 2;
        }

        if (g_shall_exit == 1) {
            WARN("notify worker processes to direct exit");
            send_signal_to_workers(TERMINATE_SIGNAL);
            g_shall_exit = 2;
        }

        if (all_workers_exit) {
            WARN("cserv exit now...");
            log_scan_write();
            delete_pidfile();
            exit(0);
        }

        if (shall_create_worker) {
            shall_create_worker = false;
            spawn_worker_process();
            if (worker_pid != mastr_pid)
                break;
        }

        log_scan_write();
        usleep(10000);
    }
}

void worker_exit_handler(int pid)
{
    for (int i = 0; i < g_worker_processes; i++) {
        struct process *p = &worker[i];
        if (p->pid == pid)
            p->pid = INVALID_PID;
    }

    /* If worker process exits without receiving the signal, it means
     * abnormal termination.
     */
    if (!g_shall_stop && !g_shall_exit)
        shall_create_worker = true;

    if (worker_empty() && (g_shall_stop || g_shall_exit))
        all_workers_exit = true;
}

void register_service(master_init_proc_t master_proc,
                      worker_init_proc_t worker_proc,
                      request_handler_t handler)
{
    master_init_proc = master_proc;
    worker_init_proc = worker_proc;
    request_handler = handler;
}

static int create_tcp_server(const char *ip, int port)
{
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        printf("socket failed. %d %s\n", errno, strerror(errno));
        exit(0);
    }

    if (set_reuse_addr(listenfd)) {
        printf("set reuse listen socket failed. %d %s\n", errno,
               strerror(errno));
        exit(0);
    }

    if (set_nonblock(listenfd)) {
        printf("set listen socket non-bloack failed. %d %s\n", errno,
               strerror(errno));
        exit(0);
    }

    struct sockaddr_in svraddr;
    memset(&svraddr, 0, sizeof(svraddr));
    svraddr.sin_family = AF_INET;
    svraddr.sin_port = htons(port);
    svraddr.sin_addr.s_addr = ip_to_nl(ip);

    if (0 != bind(listenfd, (struct sockaddr *) &svraddr, sizeof(svraddr))) {
        printf("bind failed. %d %s\n", errno, strerror(errno));
        exit(0);
    }

    if (0 != listen(listenfd, 1000)) {
        printf("listen failed. %d %s\n", errno, strerror(errno));
        exit(0);
    }

    return listenfd;
}

void tcp_srv_init()
{
    listen_fd = create_tcp_server(g_server_addr, g_server_port);
    accept_lock = shm_alloc(sizeof(spinlock_t));
    if (!accept_lock) {
        printf("Failed to alloc global accept lock\n");
        exit(0);
    }
}

void process_init()
{
    worker_pid = getpid();
    mastr_pid = worker_pid;
    g_process_type = MASTER_PROCESS;
    set_proc_title("cserv: master process");
    create_pidfile(mastr_pid);
    if (log_worker_alloc(worker_pid) < 0) {
        printf("Failed to alloc log for process:%d\n", worker_pid);
        exit(0);
    }

    for (int i = 0; i < g_worker_processes; i++) {
        struct process *p = &worker[i];
        p->pid = INVALID_PID;
        p->cpuid = i % get_ncpu();
    }
}
