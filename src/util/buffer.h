#ifndef UTIL_BUFFER_H
#define UTIL_BUFFER_H

#include <stdint.h>

struct buffer {
    uint8_t *start, *end;
    uint8_t *pos, *last;
};

static inline void bind_buffer(struct buffer *b, void *buff, size_t len)
{
    b->start = buff;
    b->end = b->start + len;
    b->pos = b->last = b->start;
}

#endif
