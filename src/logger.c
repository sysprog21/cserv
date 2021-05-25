#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "env.h"
#include "internal.h"
#include "logger.h"
#include "util/cirbuf.h"
#include "util/shm.h"
#include "util/spinlock.h"
#include "util/str.h"
#include "util/system.h"

#define LOG_SIZE (1024 * 1024)

struct log_desc {
    char msg[1024];
    struct logger *log;
    const char *file;
    int line;
    enum LOG_LEVEL level;
};

struct log_buff {
    size_t len;
    char buff[1024 - sizeof(size_t)];
};

struct logger {
    int pid;        /* pid associated with process */
    bool need_init; /* if true, server would flush logs before termination. */
    struct cirbuf queue;
};

static char STR_LOG_LEVEL[][8] = {"CRIT", "ERR", "WARN", "INFO"};

static int logger_id; /* per process */
static int logger_fd;
static enum LOG_LEVEL log_level;

static char *flush_page;      /* unit: PAGE_SIZE */
static size_t flush_page_len; /* length of used page */

static spinlock_t *logger_lock;   /* necessary when process (de-)allocation */
static int logger_num;            /* managed by master and workers */
static struct logger *log_object; /* points to the shared memory */

static enum LOG_LEVEL log_str_level(const char *level)
{
    for (size_t index = 0; index < ARRAY_SIZE(STR_LOG_LEVEL); index++) {
        if (str_equal(level, STR_LOG_LEVEL[index]))
            return (enum LOG_LEVEL) index;
    }

    printf("unknow log level. %s\n", level);
    exit(0);
}

/* FIXME: move the prototype to dedicated header */
extern ssize_t (*real_sys_write)(int fd, const void *buf, size_t count);

static void log_flush()
{
    if (0 == flush_page_len)
        return;

    if (likely(logger_fd > 0))
        real_sys_write(logger_fd, flush_page, flush_page_len);
    flush_page_len = 0;
}

static void log_read(void *data, void *args __UNUSED)
{
    struct log_buff *buff = data;

    if (buff->len > get_page_size() - flush_page_len)
        log_flush();

    memcpy(flush_page + flush_page_len, buff->buff, buff->len);
    flush_page_len += buff->len;
}

static void log_write(void *data, void *args)
{
    struct log_buff *buff = data;
    struct log_desc *desc = args;
    struct tm *tp = get_tm();
    char strnow[64];

    strftime(strnow, sizeof(strnow), "%Y-%m-%d %H:%M:%S", tp);
    buff->len =
        snprintf(buff->buff, sizeof(buff->buff), "(%d)%s %s:%d [%s]: %s\n",
                 desc->log->pid, strnow, desc->file, desc->line,
                 STR_LOG_LEVEL[desc->level], desc->msg);
}

static inline void copy_to_cache(struct logger *log)
{
    while (-1 != cirbuf_read(&log->queue, NULL))
        ;

    if (1 == log->need_init) {
        log->pid = 0;
        log->need_init = false;
    }
}

/* Scan the shared memory associated with the logging and write to disk. */
void log_scan_write()
{
    for (int index = 0; index < logger_num; index++) {
        if (log_object[index].pid == 0)
            continue;

        copy_to_cache(&log_object[index]);
    }

    log_flush();
}

/* Return 0 if success. Negative vales if failure. */
int log_worker_alloc(int pid)
{
    spin_lock(logger_lock);
    for (int index = 0; index < logger_num; index++) {
        struct logger *log = &log_object[index];

        if (log->pid == 0) {
            log->pid = pid;
            log->need_init = false;
            cirbuf_init(&log->queue, LOG_SIZE / sizeof(struct log_buff),
                        sizeof(struct log_buff), log_read, log_write);

            logger_id = index;
            spin_unlock(logger_lock);
            return 0;
        }
    }

    spin_unlock(logger_lock);

    return -1;
}

/* Call this function when worker process is about to exit */
void log_worker_flush_and_reset(int pid)
{
    spin_lock(logger_lock);
    for (int index = 0; index < logger_num; index++) {
        if (log_object[index].pid == pid)
            log_object[index].need_init = true;
    }
    spin_unlock(logger_lock);
}

static struct log_desc desc;

void log_out(enum LOG_LEVEL level,
             const char *file,
             int line,
             const char *fmt,
             ...)
{
    if (level > log_level)
        return;

    desc.log = &log_object[logger_id];
    desc.file = file, desc.line = line;
    desc.level = level;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(desc.msg, sizeof(desc.msg), fmt, ap);
    va_end(ap);

    cirbuf_write(&desc.log->queue, &desc);
}

static int gen_logfile_fd(const char *path)
{
    int fd;

    if (access(path, F_OK | R_OK | W_OK) == 0)
        fd = open(path, O_RDWR | O_APPEND, 0644);
    else
        fd = open(path, O_CREAT | O_RDWR | O_APPEND, 0644);

    if (fd == -1) {
        printf("Failed to open %s: %s\n", path, strerror(errno));
        exit(0);
    }

    return fd;
}

void log_init()
{
    logger_fd = gen_logfile_fd(g_log_path);
    log_level = log_str_level(g_log_strlevel);
    flush_page = mmap(NULL, get_page_size(), PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!flush_page) {
        printf("Failed to allocate page for log flush\n");
        exit(0);
    }

    flush_page_len = 0;
    logger_lock = shm_alloc(sizeof(spinlock_t));
    spin_lock_init(logger_lock);

    logger_num = g_worker_processes + 1; /* master process included */
    log_object = shm_alloc(sizeof(struct logger) * logger_num);
    if (!log_object) {
        printf("Failed to allocate memory for logger\n");
        exit(0);
    }

    for (int index = 0; index < logger_num; index++) {
        log_object[index].pid = 0;
        log_object[index].queue.elem =
            shm_pages_alloc(LOG_SIZE / get_page_size());
        if (!log_object[index].queue.elem) {
            printf("Failed to allocate pages for each logger\n");
            exit(0);
        }
    }
}
