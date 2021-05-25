#ifndef CORE_LOGGER_H
#define CORE_LOGGER_H

enum LOG_LEVEL {
    LEVEL_CRIT = 0,
    LEVEL_ERR = 1,
    LEVEL_WARN = 2,
    LEVEL_INFO = 3,
};

#define CRIT(fmt, ...) \
    log_out(LEVEL_CRIT, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ERR(fmt, ...) log_out(LEVEL_ERR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) \
    log_out(LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define INFO(fmt, ...) \
    log_out(LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

int log_worker_alloc(int pid);
void log_worker_flush_and_reset(int pid);
void log_scan_write();
void log_out(enum LOG_LEVEL level,
             const char *file,
             int line,
             const char *fmt,
             ...);
void log_init();

#endif
