#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "internal.h"
#include "util/system.h"

static char **__argv;
extern char **environ;

int bind_cpu(int cpuid)
{
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask); /* CPU_SET sets only the bit corresponding to cpu. */

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
        return -1;

    return 0;
}

void sys_daemon()
{
    if (fork() > 0)
        exit(0);
}

void set_proc_title(const char *name)
{
    strcpy(__argv[0], name);
}

void proc_title_init(char **argv)
{
    size_t len = 0;
    __argv = argv;

    for (int i = 1; argv[i]; i++)
        len += strlen(argv[i]) + 1;

    for (int i = 0; environ[i]; i++)
        len += strlen(environ[i]) + 1;

    void *p = malloc(len);
    if (!p) {
        printf("Failed to allocate environ memory\n");
        exit(0);
    }

    memcpy(p, argv[1] ? argv[1] : environ[0], len);

    len = 0;
    for (int i = 1; argv[i]; i++) {
        argv[i] = (char *) p + len;
        len += strlen(argv[i]) + 1;
    }

    for (int i = 0; environ[i]; i++) {
        environ[i] = (char *) p + len;
        len += strlen(environ[i]) + 1;
    }
}

void create_pidfile(int pid)
{
    FILE *fp = fopen(MASTER_PID_FILE, "w+");
    if (!fp) {
        printf("%s %s\n", strerror(errno), MASTER_PID_FILE);
        exit(0);
    }

    fprintf(fp, "%d", pid);
    fclose(fp);
}

int read_pidfile()
{
    char buff[32];
    FILE *fp = fopen(MASTER_PID_FILE, "r");
    if (!fp) {
        printf("Failed to open %s: %s\n", MASTER_PID_FILE, strerror(errno));
        exit(0);
    }

    if (!fgets(buff, sizeof(buff) - 1, fp)) {
        fclose(fp);
        exit(0);
    }

    fclose(fp);
    return atoi(buff);
}

void delete_pidfile()
{
    struct stat statfile;

    if (stat(MASTER_PID_FILE, &statfile) == 0)
        remove(MASTER_PID_FILE);
}

struct tm *get_tm()
{
    time_t t;

    time(&t);
    return localtime(&t);
}

long long get_curr_mseconds()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void sys_rlimit_init()
{
    struct rlimit rlim, rlim_new;

    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
        rlim_new.rlim_cur = rlim_new.rlim_max = 100000;
        if (setrlimit(RLIMIT_NOFILE, &rlim_new) != 0) {
            printf("Failed to set rlimit file. exit!\n");
            exit(0);
        }
    }

    if (getrlimit(RLIMIT_CORE, &rlim) == 0) {
        rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &rlim_new) != 0) {
            printf("Failed to set rlimit core. exit!\n");
            exit(0);
        }
    }
}

static int page_size = 4096;
static int cpu_num = 1;

__INIT static void system_init()
{
    page_size = sysconf(_SC_PAGESIZE);
    cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    sys_rlimit_init();
}

__CONST int get_page_size()
{
    return page_size;
}

__CONST int get_ncpu()
{
    return cpu_num;
}
