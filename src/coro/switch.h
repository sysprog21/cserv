#pragma once

struct coro_stack {
    void *ptr;
    size_t size_bytes;
};

struct context {
    void **sp; /* current coroutine's top stack. equivalent to %esp / %rsp */
};

typedef void (*coro_routine)(void *args) ATTRIBUTE_REGPARM(1);

void context_switch(struct context *prev, struct context *next)
    __attribute__((__noinline__)) ATTRIBUTE_REGPARM(2);

void coro_stack_init(struct context *ctx,
                     struct coro_stack *stack,
                     coro_routine routine,
                     void *args);
int coro_stack_alloc(struct coro_stack *stack, size_t size_bytes);
void coro_stack_free(struct coro_stack *stack);
