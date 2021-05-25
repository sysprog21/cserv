#ifndef UTIL_SYSTEM_H
#define UTIL_SYSTEM_H

#include <sys/time.h>

#include "internal.h"

int get_page_size() __CONST;
int get_ncpu() __CONST;

int bind_cpu(int cpuid);
void sys_daemon();
void set_proc_title(const char *name);
void proc_title_init(char **argv);
void create_pidfile(int pid);
int read_pidfile();
void delete_pidfile();

struct tm *get_tm();
long long get_curr_mseconds();

#endif
