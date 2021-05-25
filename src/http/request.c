#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "env.h"
#include "logger.h"
#include "util/hashtable.h"
#include "util/memcache.h"
#include "util/net.h"
#include "util/str.h"

#include "http/parse.h"
#include "http/request.h"
#include "http/response.h"

static size_t client_header_size;
static struct memcache *http_request_cache;

static struct request_line_handler request_line_handler = {NULL, NULL, NULL};
static struct hash_table *request_header_ht;

static void request_body_default_handler(struct http_request *r)
{
    http_finalize_request(r, 200);
}

static http_request_body_handler_t request_body_handler =
    request_body_default_handler;

static int http_process_content_length(struct http_request *r)
{
    r->content_len = str_atoi(&r->header_value);
    if (r->content_len <= 0)
        return -1;

    return 0;
}

static int http_process_host(struct http_request *r)
{
    enum { sw_usual = 0, sw_literal, sw_rest } state = sw_usual;

    size_t dot_pos = r->header_value.len;
    size_t host_len = dot_pos;
    r->host.p = r->header_value.p;

    for (size_t i = 0; i < r->header_value.len; i++) {
        unsigned char ch = r->header_value.p[i];
        switch (ch) {
        case '.':
            if (dot_pos == i - 1)
                return -1;
            dot_pos = i;
            break;
        case ':':
            if (state == sw_usual) {
                host_len = i;
                state = sw_rest;
            }
            break;
        case '[':
            if (i == 0)
                state = sw_literal;
            break;
        case ']':
            if (state == sw_literal) {
                host_len = i + 1;
                state = sw_rest;
            }
            break;
        case '\0':
            return -1;
        default:
            if (ch == '/')
                return -2;
            if (ch >= 'A' && ch <= 'Z')
                r->header_value.p[i] = tolower(ch);
            break;
        }
    }

    if (dot_pos == host_len - 1)
        host_len--;
    if (host_len == 0)
        return -3;

    r->host.len = host_len;

    return 0;
}

static struct request_header_handler http_headers_handler[] = {
    {STRING("Host"), http_process_host},
    {STRING("Connection"), NULL},
    {STRING("If-Modified-Since"), NULL},
    {STRING("If-Unmodified-Since"), NULL},
    {STRING("If-Match"), NULL},
    {STRING("If-None-Match"), NULL},
    {STRING("User-Agent"), NULL},
    {STRING("Referer"), NULL},
    {STRING("Content-Length"), http_process_content_length},
    {STRING("Content-Type"), NULL},
    {STRING("Range"), NULL},
    {STRING("If-Range"), NULL},
    {STRING("Transfer-Encoding"), NULL},
    {STRING("Expect"), NULL},
    {STRING("Upgrade"), NULL},
    {STRING("Authorization"), NULL},
    {STRING("Keep-Alive"), NULL},
    {STRING("Cookie"), NULL},
    {NULL_STRING, NULL},
};

static int recv_http_request(int fd, struct buffer *b)
{
    int nread = -1;

    if (b->last == b->end)
        return -1;

    nread = recv(fd, b->last, b->end - b->last, 0);
    if (nread < 0) {
        if (ETIME == errno) {
            ERR("recv timeout, client ip:%s", get_peer_ip(fd));
            return -2;
        } else {
            ERR("recv http request error:%d errno:%d %s", nread, errno,
                strerror(errno));
            return -3;
        }
    } else if (nread == 0) {
        ERR("closed by client. client addr: %s", get_peer_ip(fd));
        return -4;
    }

    b->last += nread;

    return 0;
}

static int http_process_request_line(struct http_request *r)
{
    struct buffer *b = &r->header;

    for (;;) {
        int ret = recv_http_request(r->fd, b);
        if (ret) {
            ERR("recv http request line error:%d", ret);
            return -1;
        }

        ret = http_parse_request_line(r, b);
        if (ret == 0) /* successful */
            return 0;
        if (ret > 0)
            continue;

        /* encounter errors */
        ERR("parse request line error:%d", ret);
        return -2;
    }
}

static int handle_http_request_header(struct http_request *r)
{
    if (r->invalid_header)
        return 0;

    struct request_header_handler *hh =
        hash_table_find(request_header_ht, r->header_hash);
    if (hh && hh->handler)
        return hh->handler(r);

    return 0;
}

static int http_process_request_header(struct http_request *r)
{
    struct buffer *b = &r->header;

    for (;;) {
        int ret = http_parse_request_header(r, b);
        switch (ret) {
        case 0: /* successful */
            if (handle_http_request_header(r))
                return -1;
            continue;
        case 1: /* continue */
            ret = recv_http_request(r->fd, b);
            if (ret) {
                ERR("recv http request header error:%d", ret);
                return -2;
            }
            continue;
        case 100:
            return 0;
        default: /* error */
            ERR("parse request header error:%d", ret);
            return -3;
        }
    }
}

static int http_process_request_body(struct http_request *r)
{
    struct buffer *b = &r->header;

    while (b->last - b->pos != r->content_len) {
        int ret = recv_http_request(r->fd, b);
        if (ret) {
            ERR("recv http request content error:%d", ret);
            return -1;
        }
    }

    r->content.p = b->pos;
    r->content.len = r->content_len;

    return 0;
}

static void __http_request_handler(struct http_request *r)
{
    if (http_process_request_line(r)) {
        http_finalize_request(r, HTTP_BAD_REQUEST);
        return;
    }

    if (request_line_handler.method && request_line_handler.method(r))
        return;

    if (request_line_handler.uri && request_line_handler.uri(r))
        return;

    if (request_line_handler.http && request_line_handler.http(r))
        return;

    if (r->http_version < HTTP_VER_10) {
        request_body_handler(r);
        return;
    }

    if (http_process_request_header(r)) {
        http_finalize_request(r, HTTP_BAD_REQUEST);
        return;
    }

    if (http_process_request_body(r)) {
        http_finalize_request(r, HTTP_INSUFFICIENT_STORAGE);
        return;
    }

    request_body_handler(r);
}

void http_request_handler(int fd)
{
    struct http_request *r = memcache_alloc(http_request_cache);
    if (!r) {
        ERR("no mem for http request");
        http_fast_response(fd, HTTP_INSUFFICIENT_STORAGE_PAGE,
                           sizeof(HTTP_INSUFFICIENT_STORAGE_PAGE) - 1);
        return;
    }

    r->fd = fd;
    r->state = 0;
    str_init(&r->request_line);
    r->args = NULL;
    str_init(&r->exten);
    str_init(&r->http_protocol);
    r->content_len = 0;

    bind_buffer(&r->header, (char *) r + sizeof(struct http_request),
                client_header_size - sizeof(struct http_request));

    __http_request_handler(r);
    memcache_free(http_request_cache, r);
}

static unsigned hash_key_lc(unsigned char *data, size_t len)
{
    unsigned key = 0;

    for (size_t i = 0; i < len; i++)
        key = HASH(key, tolower(data[i]));

    return key;
}

static void add_client_header_handler(struct request_header_handler *handler)
{
    if (!handler)
        return;

    for (; handler->name.len; handler++) {
        if (!handler->handler)
            continue;

        unsigned key = hash_key_lc(handler->name.p, handler->name.len);
        if (hash_table_add(request_header_ht, key, handler)) {
            ERR("r handler conflict. key:%u, handler:%.*s", key,
                handler->name.len, handler->name.p);
            exit(-1);
        }
    }
}

void http_request_init(size_t client_header_buffer_kbytes,
                       struct request_line_handler *line_handler,
                       struct request_header_handler *header_handler,
                       http_request_body_handler_t body_handler)
{
    client_header_size = client_header_buffer_kbytes << 10;

    if (line_handler)
        memcpy(&request_line_handler, line_handler,
               sizeof(struct request_line_handler));

    http_request_cache =
        memcache_create(client_header_size, g_worker_connections);
    if (!http_request_cache) {
        ERR("Failed to create memcache for HTTP request header");
        exit(-1);
    }

    request_header_ht = hash_table_create(128);
    if (!request_header_ht) {
        ERR("Failed to create hash table for HTTP request header");
        exit(-2);
    }

    add_client_header_handler(http_headers_handler);
    add_client_header_handler(header_handler);
    if (body_handler)
        request_body_handler = body_handler;
}
