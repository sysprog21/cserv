#include <stdlib.h>
#include <sys/mman.h>

#include "coroutine/switch.h"
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
#elif defined(__i386__)
#define NUM_SAVED 4
    "push %ebp\n"
    "push %ebx\n"
    "push %esi\n"
    "push %edi\n"
    "mov %esp, (%eax)\n"
    "mov (%edx), %esp\n"
    "pop %edi\n"
    "pop %esi\n"
    "pop %ebx\n"
    "pop %ebp\n"
    "pop %ecx\n"
    "jmp *%ecx\n"
#else
#error "unsupported architecture"
#endif
);

void coro_routine_entry();

__asm__(
    ".text\n"
    "coro_routine_entry:\n"
#if defined(__x86_64__)
    "pop %rdi\n"
    "pop %rcx\n"
    "call *%rcx\n"
#elif defined(__i386__)
    "pop %eax\n"
    "pop %ecx\n"
    "call *%ecx\n"
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
