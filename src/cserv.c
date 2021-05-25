#include <stdio.h>
#include <stdlib.h>

#include "env.h"
#include "logger.h"
#include "process.h"
#include "util/conf.h"
#include "util/shm.h"
#include "util/str.h"
#include "util/system.h"

static void usage()
{
    printf(
        "Usage: cserv [command]\n"
        "command options:\n"
        "  conf  : check and show config file content\n"
        "  start : start the web server\n"
        "  stop  : stop accepting and wait for termination\n"
        "  exit  : forcely exit\n");
}

static void send_signal(int signo)
{
    int pid = read_pidfile();
    kill(pid, signo);
}

static void handle_cmds(int argc, char *argv[])
{
    if (argc != 2) {
        usage();
        exit(0);
    }

    if (str_equal(argv[1], "conf")) {
        load_conf(CONF_FILE);
        conf_env_init();
        print_env();
        return;
    }

    if (str_equal(argv[1], "stop")) {
        send_signal(SHUTDOWN_SIGNAL);
        return;
    }

    if (str_equal(argv[1], "exit")) {
        send_signal(TERMINATE_SIGNAL);
        return;
    }

    /* unsupported command */
    if (!str_equal(argv[1], "start")) {
        fprintf(stderr, "Unsupport command: %s\n\n", argv[1]);
        usage();
        exit(1);
    }

    /* start the service */
    sys_daemon();
    proc_title_init(argv);

    load_conf(CONF_FILE);
    conf_env_init();
    shm_init();
    log_init();

    tcp_srv_init();
    process_init();
    master_process_cycle();
    worker_process_cycle();
}

int main(int argc, char **argv)
{
    handle_cmds(argc, argv);

    return 0;
}
