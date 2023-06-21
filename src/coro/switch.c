#include <stdlib.h>
#include <sys/mman.h>

#include "coro/switch.h"
#include "util/system.h"

__asm__(
    ".text\n"
    ".globl context_switch\n"
    "context_switch:\n"
#if defined(__x86_64__)
#define NUM_SAVED 6
    "push %rbp\n"
    "push %rbx\n"
    "push %r12\n"
    "push %r13\n"
    "push %r14\n"
    "push %r15\n"
    "mov %rsp, (%rdi)\n"
    "mov (%rsi), %rsp\n"
    "pop %r15\n"
    "pop %r14\n"
    "pop %r13\n"
    "pop %r12\n"
    "pop %rbx\n"
    "pop %rbp\n"
    "pop %rcx\n"
    "jmp *%rcx\n"
#elif defined(__aarch64__)
#define NUM_SAVED 11
    "sub sp, sp, #96\n"
    "mov x8, sp\n"
    "stp x19, x20, [x8], #16\n"  // store callee-saved registers
    "stp x21, x22, [x8], #16\n"
    "stp x23, x24, [x8], #16\n"
    "stp x25, x26, [x8], #16\n"
    "stp x27, x28, [x8], #16\n"
    "stp x29, lr, [x8], #16\n"
    "mov x8, sp\n"
    "str x8, [x0]\n"
    "ldr x8, [x1]\n"
    "mov sp, x8\n"
    "ldp x19, x20, [x8], #16\n"  // restore callee-saved registers
    "ldp x21, x22, [x8], #16\n"
    "ldp x23, x24, [x8], #16\n"
    "ldp x25, x26, [x8], #16\n"
    "ldp x27, x28, [x8], #16\n"
    "ldp x29, lr, [x8], #16\n"
    "mov sp, x8\n"
    "br lr\n"
#else
#error "unsupported architecture"
#endif
);

void coro_routine_entry();

__asm__(
    ".text\n"
#if defined(__aarch64__)
    ".globl coro_routine_entry\n"
#endif
    "coro_routine_entry:\n"
#if defined(__x86_64__)
    "pop %rdi\n"
    "pop %rcx\n"
    "call *%rcx\n"
#elif defined(__aarch64__)
    "mov x8, sp\n"
    "ldp x0, x9, [x8], #16\n"
    "mov sp, x8\n"
    "blr x9\n"
#else
#error "unsupported architecture"
#endif
);

void coro_stack_init(struct context *ctx,
                     struct coro_stack *stack,
                     coro_routine routine,
                     void *args)
{
    ctx->sp = (void **) stack->ptr;
    *--ctx->sp = (void *) routine;
    *--ctx->sp = (void *) args;
    *--ctx->sp = (void *) coro_routine_entry;
    ctx->sp -= NUM_SAVED;
}

/* ensure size_bytes aligned by get_page_size() */
int coro_stack_alloc(struct coro_stack *stack, size_t size_bytes)
{
    stack->size_bytes = size_bytes;
    size_bytes += get_page_size();

    stack->ptr = mmap(NULL, size_bytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack->ptr == (void *) -1)
        return -1;

    mprotect(stack->ptr, get_page_size(), PROT_NONE);
    stack->ptr = (void *) ((char *) stack->ptr + size_bytes);

    return 0;
}

void coro_stack_free(struct coro_stack *stack)
{
    stack->ptr =
        (void *) ((char *) stack->ptr - stack->size_bytes - get_page_size());
    munmap(stack->ptr, stack->size_bytes + get_page_size());
}
