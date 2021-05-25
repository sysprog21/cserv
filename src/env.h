#ifndef CORE_ENV_H
#define CORE_ENV_H

/* associated with configurations */
extern char *g_log_path;
extern char *g_log_strlevel;

extern int g_worker_processes;
extern int g_worker_connections;
extern int g_coro_stack_kbytes;

extern char *g_server_addr;
extern int g_server_port;

void print_env();
void conf_env_init();

#endif
