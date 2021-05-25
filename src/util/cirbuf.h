#ifndef UTIL_CIRBUF_H
#define UTIL_CIRBUF_H

/* circular buffer implementation */

#include <stddef.h>

typedef void (*cirbuf_rw_cb)(void *data, void *args);

struct cirbuf {
    int elem_count;
    size_t elem_size;
    int start, end;
    cirbuf_rw_cb read_cb /* consumer cb */, write_cb /* producer cb */;
    void *elem;
};

static inline int cirbuf_is_empty(struct cirbuf *queue)
{
    return queue->start == queue->end;
}

static inline int cirbuf_is_full(struct cirbuf *queue)
{
    return (queue->end + 1) % queue->elem_count == queue->start;
}

/* producer */
static inline int cirbuf_write(struct cirbuf *queue, void *args)
{
    void *data;

    if (cirbuf_is_full(queue))
        return -1;

    data = (char *) queue->elem + queue->end * queue->elem_size;
    queue->write_cb(data, args);
    queue->end = (queue->end + 1) % queue->elem_count;

    return 0;
}

/* consumer */
static inline int cirbuf_read(struct cirbuf *queue, void *args)
{
    void *data;

    if (cirbuf_is_empty(queue))
        return -1;

    data = (char *) queue->elem + queue->start * queue->elem_size;
    queue->read_cb(data, args);
    queue->start = (queue->start + 1) % queue->elem_count;

    return 0;
}

/* Initilaize the circular buffer.
 * @elem_count: element count
 * @elem_size: size of each element
 *
 * NOTE:
 * 1. queue should be allocated before invoking this function.
 * 2. The capacity is (elem_count - 1) because one is used for check full/empty
 */
static inline void cirbuf_init(struct cirbuf *queue,
                               int elem_count,
                               size_t elem_size,
                               cirbuf_rw_cb read_cb,
                               cirbuf_rw_cb write_cb)
{
    queue->elem_count = elem_count;
    queue->elem_size = elem_size;
    queue->start = 0;
    queue->end = 0;
    queue->read_cb = read_cb;
    queue->write_cb = write_cb;
}
#endif
