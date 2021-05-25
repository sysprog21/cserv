#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "util/buffer.h"
#include "util/str.h"

enum http_method {
    HTTP_UNKNOWN = 0x0001,
    HTTP_GET = 0x0002,
    HTTP_HEAD = 0x0004,
    HTTP_POST = 0x0008,
    HTTP_PUT = 0x0010,
    HTTP_DELETE = 0x0020,
    HTTP_MKCOL = 0x0040,
    HTTP_COPY = 0x0080,
    HTTP_MOVE = 0x0100,
    HTTP_OPTIONS = 0x0200,
    HTTP_PROPFIND = 0x0400,
    HTTP_PROPPATCH = 0x0800,
    HTTP_LOCK = 0x1000,
    HTTP_UNLOCK = 0x2000,
    HTTP_PATCH = 0x4000,
    HTTP_TRACE = 0x8000,
};

#define HTTP_VER_09 9
#define HTTP_VER_10 1000
#define HTTP_VER_11 1001

struct http_header {
    unsigned hash;
    str_t key, value;
    char *lowcase_key;
};

struct http_headers_in {
    struct http_header host;
    struct http_header connection;
    struct http_header if_modified_since;
    struct http_header if_unmodified_since;
    struct http_header if_match;
    struct http_header if_none_match;
    struct http_header user_agent;
    struct http_header referer;
    struct http_header content_length;
    struct http_header content_type;

    struct http_header range;
    struct http_header if_range;

    struct http_header transfer_encoding;
    struct http_header expect;
    struct http_header upgrade;

    struct http_header authorization;

    struct http_header keep_alive;
};

#define http_header_exist(header) (header->key.p)
#define HTTP_LC_HEADER_LEN 32

struct http_request {
    int fd;
    struct buffer header;

    int state; /* state machine */

    str_t request_line;
    str_t method_name;
    enum http_method method;
    str_t schema; /* schema://host:port/uri */
    str_t host;
    str_t port;
    str_t uri;
    unsigned char *args;
    str_t exten;
    str_t http_protocol;
    unsigned http_major : 16;
    unsigned http_minor : 16;
    int http_version;

    unsigned complex_uri : 1;  /* URI with "/." and on Win32 with "//" */
    unsigned quoted_uri : 1;   /* URI with "%" */
    unsigned plus_in_uri : 1;  /* URI with "+" */
    unsigned space_in_uri : 1; /* URI with " " */

    int content_len;
    str_t content;

    /* used to parse HTTP headers */
    str_t header_name;
    str_t header_value;
    int invalid_header; /* "0" means valid */
    unsigned header_hash;
    unsigned lowcase_index;
    char lowcase_header[HTTP_LC_HEADER_LEN];
};

/* request line */
typedef int (*http_request_line_handler_t)(struct http_request *r);
struct request_line_handler {
    http_request_line_handler_t method;
    http_request_line_handler_t uri;
    http_request_line_handler_t http;
};

/* request header */
typedef int (*http_request_header_handler_t)(struct http_request *r);
struct request_header_handler {
    str_t name;
    http_request_header_handler_t handler;
};

/* request body, the rest */
typedef void (*http_request_body_handler_t)(struct http_request *r);

void http_request_handler(int fd);
void http_request_init(size_t client_header_buffer_kbytes,
                       struct request_line_handler *line_handler,
                       struct request_header_handler *header_handler,
                       http_request_body_handler_t body_handler);

#endif
