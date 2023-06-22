#pragma once

#include <string.h>

#define CR (unsigned char) 13
#define LF (unsigned char) 10
#define CRLF "\x0d\x0a"

typedef struct {
    unsigned char *p;
    size_t len;
} str_t;

#define STRING(str)                            \
    {                                          \
        (unsigned char *) str, sizeof(str) - 1 \
    }
#define NULL_STRING \
    {               \
        NULL, 0     \
    }

static inline void str_init(str_t *str)
{
    str->p = NULL;
    str->len = 0;
}

static inline int str_equal(const char *s, const char *t)
{
    return !strcmp(s, t);
}

static inline int str_atoi(str_t *s)
{
    int val = 0;

    for (size_t i = 0; i < s->len; i++) {
        switch (s->p[i]) {
        case '0' ... '9':
            val = 10 * val + (s->p[i] - '0');
            break;
        default:
            return val;
        }
    }

    return val;
}
