#include <stdio.h>
#include <stdlib.h>

#include "env.h"
#include "logger.h"
#include "process.h"
#include "util/conf.h"
#include "util/shm.h"
#include "util/str.h"
#include "util/system.h"

static void usage(const char *prog)
{
    printf(
        "Usage: %s [command]\n"
        "command options:\n"
        "  conf  : check and show config file content\n"
        "  start : start the web server\n"
        "  stop  : stop accepting and wait for termination\n"
        "  exit  : force quit the server\n",
        prog);
}

static void send_signal(int signo)
{
    int pid = read_pidfile();
    kill(pid, signo);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return 0;
    }

    if (str_equal(argv[1], "conf")) {
        load_conf(CONF_FILE);
        conf_env_init();
        print_env();
        return 0;
    }

    if (str_equal(argv[1], "stop")) {
        send_signal(SHUTDOWN_SIGNAL);
	printf("cserv: stopped.\n");
        return 0;
    }

    if (str_equal(argv[1], "exit")) {
        send_signal(TERMINATE_SIGNAL);
	printf("cserv: force quit.\n");
        return 0;
    }

    /* unsupported command */
    if (!str_equal(argv[1], "start")) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        usage(argv[0]);
        return 1;
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
    printf("cserv: started.\n");

    master_process_cycle();
    worker_process_cycle();

    return 0;
}
