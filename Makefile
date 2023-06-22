.PHONY: all clean
TARGET = cserv
GIT_HOOKS := .git/hooks/applied
all: $(TARGET)

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

include common.mk

CFLAGS = -I./src
CFLAGS += -O2 -g
CFLAGS += -std=gnu11 -Wall -W
CFLAGS += -include src/common.h

# Configurations
CFLAGS += -D CONF_FILE="\"conf/cserv.conf\""
CFLAGS += -D MASTER_PID_FILE="\"conf/cserv.pid\""
CFLAGS += -D MAX_WORKER_PROCESS=64

LDFLAGS = -ldl

# standard build rules
.SUFFIXES: .o .c
.c.o:
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF $@.d $<

OBJS = \
	src/util/conf.o \
	src/util/hashtable.o \
	src/util/shm.o \
	src/util/memcache.o \
	src/util/rbtree.o \
	src/util/system.o \
	src/http/http.o \
	src/http/parse.o \
	src/http/request.o \
	src/http/response.o \
	src/coro/switch.o \
	src/coro/sched.o \
	src/env.o \
	src/event.o \
	src/signal.o \
	src/logger.o \
	src/process.o \
	src/syscall_hook.o \
	src/main.o

deps += $(OBJS:%.o=%.o.d)

$(TARGET): $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(VECHO) "  Cleaning...\n"
	$(Q)$(RM) $(TARGET) $(OBJS) $(deps)

-include $(deps)
