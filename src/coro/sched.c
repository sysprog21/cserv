#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coro/sched.h"
#include "coro/switch.h"
#include "event.h"
#include "internal.h"
#include "util/list.h"
#include "util/memcache.h"
#include "util/rbtree.h"
#include "util/system.h"

struct timer_node {
    struct rb_node node; /* inactive */
    struct rb_root root; /* coroutines with the same timeout */
    long long timeout;   /* unit: milliseconds */
};

struct coroutine {
    struct list_head list; /* free_pool or active linked list */
    struct rb_node node;
    int coro_id;
    struct context ctx;
    struct coro_stack stack;
    coro_func func;
    void *args; /* associated with coroutine function */

    long long timeout; /* track the timeout of events */
    int active_by_timeout;
};

/* sched policy, if timeout == -1, sched policy can wait forever */
typedef void (*sched_policy_t)(int timeout /* in milliseconds */);

struct coro_schedule {
    size_t max_coro_size;
    size_t curr_coro_size;

    int next_coro_id;

    size_t stack_bytes;
    struct coroutine main_coro;
    struct coroutine *current;

    struct list_head idle, active;
    struct rb_root inactive; /* waiting coroutines */
    struct memcache *cache;  /* caching timer node */

    sched_policy_t policy;
};

static struct coro_schedule sched;

static struct coroutine *find_in_timer(struct timer_node *tm_node, int coro_id)
{
    struct rb_node *each = tm_node->root.rb_node;

    while (each) {
        struct coroutine *coro = container_of(each, struct coroutine, node);

        int result = coro_id - coro->coro_id;
        if (result < 0)
            each = each->rb_left;
        else if (result > 0)
            each = each->rb_right;
        else
            return coro;
    }

    return NULL;
}

static inline void remove_from_timer_node(struct timer_node *tm_node,
                                          struct coroutine *coro)
{
    if (!find_in_timer(tm_node, coro->coro_id))
        return;

    rb_erase(&coro->node, &tm_node->root);
}

static struct timer_node *find_timer_node(long long timeout)
{
    struct rb_node *each = sched.inactive.rb_node;

    while (each) {
        struct timer_node *tm_node =
            container_of(each, struct timer_node, node);
        long long timespan = timeout - tm_node->timeout;
        if (timespan < 0)
            each = each->rb_left;
        else if (timespan > 0)
            each = each->rb_right;
        else
            return tm_node;
    }

    return NULL;
}

static inline void remove_from_inactive_tree(struct coroutine *coro)
{
    struct timer_node *tm_node = find_timer_node(coro->timeout);
    if (!tm_node)
        return;

    remove_from_timer_node(tm_node, coro);

    if (RB_EMPTY_ROOT(&tm_node->root)) {
        rb_erase(&tm_node->node, &sched.inactive);
        memcache_free(sched.cache, tm_node);
    }
}

static void add_to_timer_node(struct timer_node *tm_node,
                              struct coroutine *coro)
{
    struct rb_node **newer = &tm_node->root.rb_node, *parent = NULL;

    while (*newer) {
        struct coroutine *each = container_of(*newer, struct coroutine, node);
        int result = coro->coro_id - each->coro_id;
        parent = *newer;

        newer = (result < 0) ? &(*newer)->rb_left : &(*newer)->rb_right;
    }

    rb_link_node(&coro->node, parent, newer);
    rb_insert_color(&coro->node, &tm_node->root);
}

static void move_to_inactive_tree(struct coroutine *coro)
{
    struct rb_node **newer = &sched.inactive.rb_node, *parent = NULL;

    while (*newer) {
        struct timer_node *tm_node =
            container_of(*newer, struct timer_node, node);
        int timespan = coro->timeout - tm_node->timeout;

        parent = *newer;
        if (timespan < 0)
            newer = &(*newer)->rb_left;
        else if (timespan > 0)
            newer = &(*newer)->rb_right;
        else {
            add_to_timer_node(tm_node, coro);
            return;
        }
    }

    struct timer_node *tmp = memcache_alloc(sched.cache);
    if (unlikely(!tmp)) /* still in active list */
        return;
    tmp->root = RB_ROOT;
    tmp->node.rb_left = NULL;
    tmp->node.rb_right = NULL;

    tmp->timeout = coro->timeout;
    add_to_timer_node(tmp, coro);
    rb_link_node(&tmp->node, parent, newer);
    rb_insert_color(&tmp->node, &sched.inactive);
}

static inline void move_to_idle_list_direct(struct coroutine *coro)
{
    list_add_tail(&coro->list, &sched.idle);
}

static inline void move_to_active_list_tail_direct(struct coroutine *coro)
{
    list_add_tail(&coro->list, &sched.active);
}

static inline void move_to_active_list_tail(struct coroutine *coro)
{
    remove_from_inactive_tree(coro);
    list_add_tail(&coro->list, &sched.active);
}

static inline void move_to_active_list_head(struct coroutine *coro)
{
    remove_from_inactive_tree(coro);
    list_add(&coro->list, &sched.active);
}

static inline void coroutine_init(struct coroutine *coro)
{
    INIT_LIST_HEAD(&coro->list);
    RB_CLEAR_NODE(&coro->node);
    coro->timeout = 0;
    coro->active_by_timeout = -1;
}

static struct coroutine *create_coroutine()
{
    if (unlikely(sched.curr_coro_size == sched.max_coro_size))
        return NULL;

    struct coroutine *coro = malloc(sizeof(struct coroutine));
    if (unlikely(!coro))
        return NULL;

    if (coro_stack_alloc(&coro->stack, sched.stack_bytes)) {
        free(coro);
        return NULL;
    }

    coro->coro_id = ++sched.next_coro_id;
    sched.curr_coro_size++;

    return coro;
}

static struct coroutine *get_coroutine()
{
    struct coroutine *coro;

    if (!list_empty(&sched.idle)) {
        coro = list_first_entry(&sched.idle, struct coroutine, list);
        list_del(&coro->list);
        coroutine_init(coro);

        return coro;
    }

    coro = create_coroutine();
    if (coro)
        coroutine_init(coro);

    return coro;
}

static void coroutine_switch(struct coroutine *curr, struct coroutine *next)
{
    sched.current = next;
    context_switch(&curr->ctx, &next->ctx);
}

static inline struct coroutine *get_active_coroutine()
{
    struct coroutine *coro;

    if (list_empty(&sched.active))
        return NULL;

    coro = list_entry(sched.active.next, struct coroutine, list);
    list_del_init(&coro->list);

    return coro;
}

static inline void run_active_coroutine()
{
    struct coroutine *coro;

    while ((coro = get_active_coroutine()))
        coroutine_switch(&sched.main_coro, coro);
}

static inline struct timer_node *get_recent_timer_node()
{
    struct rb_node *recent = rb_first(&sched.inactive);
    if (!recent)
        return NULL;

    return container_of(recent, struct timer_node, node);
}

static inline void timeout_coroutine_handler(struct timer_node *node)
{
    struct rb_node *recent;

    while ((recent = node->root.rb_node)) {
        struct coroutine *coro = container_of(recent, struct coroutine, node);
        rb_erase(recent, &node->root);
        coro->active_by_timeout = 1;
        move_to_active_list_tail_direct(coro);
    }
}

static void check_timeout_coroutine()
{
    struct timer_node *node;
    long long now = get_curr_mseconds();

    while ((node = get_recent_timer_node())) {
        if (now < node->timeout)
            return;

        rb_erase(&node->node, &sched.inactive);
        timeout_coroutine_handler(node);
        memcache_free(sched.cache, node);
    }
}

static inline int get_recent_timespan()
{
    struct timer_node *node = get_recent_timer_node();
    if (!node)
        return 10 * 1000; /* at least 10 seconds */

    int timespan = node->timeout - get_curr_mseconds();
    return (timespan < 0) ? 0 : timespan;
}

void schedule_cycle()
{
    for (;;) {
        check_timeout_coroutine();
        run_active_coroutine();

        sched.policy(get_recent_timespan());
        run_active_coroutine();
    }
}

static __attribute__((__regparm__(1))) void coro_routine_proxy(void *args)
{
    struct coroutine *coro = args;

    coro->func(coro->args);
    move_to_idle_list_direct(coro);
    coroutine_switch(sched.current, &sched.main_coro);
}

int dispatch_coro(coro_func func, void *args)
{
    struct coroutine *coro = get_coroutine();
    if (unlikely(!coro))
        return -1;

    coro->func = func, coro->args = args;

    coro_stack_init(&coro->ctx, &coro->stack, coro_routine_proxy, coro);
    move_to_active_list_tail_direct(coro);

    return 0;
}

void schedule_timeout(int milliseconds)
{
    struct coroutine *coro = sched.current;

    coro->timeout = get_curr_mseconds() + milliseconds;
    move_to_inactive_tree(coro);
    coroutine_switch(sched.current, &sched.main_coro);
}

bool is_wakeup_by_timeout()
{
    int result = sched.current->active_by_timeout;
    sched.current->active_by_timeout = -1;

    return result == 1;
}

void wakeup_coro(void *args)
{
    struct coroutine *coro = args;

    coro->active_by_timeout = -1;
    move_to_active_list_tail(coro);
}

void wakeup_coro_priority(void *args)
{
    struct coroutine *coro = args;

    coro->active_by_timeout = -1;
    move_to_active_list_head(coro);
}

void *current_coro()
{
    return sched.current;
}

/* @stack_bytes should be aligned to PAGE_SIZE */
void schedule_init(size_t stack_kbytes, size_t max_coro_size)
{
    assert(max_coro_size >= 2);

    sched.max_coro_size = max_coro_size;
    sched.curr_coro_size = 0;
    sched.next_coro_id = 0;

    sched.stack_bytes = stack_kbytes * 1024;
    sched.current = NULL;

    INIT_LIST_HEAD(&sched.idle);
    INIT_LIST_HEAD(&sched.active);
    sched.inactive = RB_ROOT;
    sched.cache = memcache_create(sizeof(struct timer_node), max_coro_size / 2);
    sched.policy = event_cycle;
    if (!sched.cache) {
        printf("Failed to create cache for schedule timer node\n");
        exit(0);
    }
}
