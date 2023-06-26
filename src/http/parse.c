#include <stdio.h>

#include "http/request.h"
#include "logger.h"
#include "util/hashtable.h"

#define CMP3(m, c0, c1, c2) (m[0] == c0 && m[1] == c1 && m[2] == c2)

#define CMP30(m, c0, c1, c2, c3) m[0] == c0 &&m[2] == c2 &&m[3] == c3

#define CMP4(m, c0, c1, c2, c3) \
    m[0] == c0 &&m[1] == c1 &&m[2] == c2 &&m[3] == c3

#define CMP5(m, c0, c1, c2, c3, c4) \
    m[0] == c0 &&m[1] == c1 &&m[2] == c2 &&m[3] == c3 &&m[4] == c4

#define CMP6(m, c0, c1, c2, c3, c4, c5) \
    m[0] == c0 &&m[1] == c1 &&m[2] == c2 &&m[3] == c3 &&m[4] == c4 &&m[5] == c5

#define CMP7(m, c0, c1, c2, c3, c4, c5, c6, c7)                              \
    m[0] == c0 &&m[1] == c1 &&m[2] == c2 &&m[3] == c3 &&m[4] == c4 &&m[5] == \
        c5 &&m[6] == c6

#define CMP8(m, c0, c1, c2, c3, c4, c5, c6, c7)                              \
    m[0] == c0 &&m[1] == c1 &&m[2] == c2 &&m[3] == c3 &&m[4] == c4 &&m[5] == \
        c5 &&m[6] == c6 &&m[7] == c7

#define CMP9(m, c0, c1, c2, c3, c4, c5, c6, c7, c8)                          \
    m[0] == c0 &&m[1] == c1 &&m[2] == c2 &&m[3] == c3 &&m[4] == c4 &&m[5] == \
        c5 &&m[6] == c6 &&m[7] == c7 &&m[8] == c8

static unsigned usual[] = {
    0xffffdbfe, /* 1111 1111 1111 1111  1101 1011 1111 1110
                 * ?>=< ;:98 7654 3210  /.-, +*)( '&%$ #"!  */
    0x7fff37d6, /* 0111 1111 1111 1111  0011 0111 1101 0110 *
                 * _^]\ [ZYX WVUT SRQP  ONML KJIH GFED CBA@ */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 *
                 *  ~}| {zyx wvut srqp  onml kjih gfed cba` */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */

    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff, /* 1111 1111 1111 1111  1111 1111 1111 1111 */
    0xffffffff  /* 1111 1111 1111 1111  1111 1111 1111 1111 */
};

enum http_request_line_status {
    S_start = 0,
    S_method,
    S_spaces_before_uri,
    S_schema,
    S_schema_slash,
    S_schema_slash_slash,
    S_host_start,
    S_host,
    S_host_end,
    S_host_ip_literal,
    S_port,
    S_host_http_09,
    S_after_slash_in_uri,
    S_check_uri,
    S_check_uri_http_09,
    S_uri,
    S_http_09,
    S_http_H,
    S_http_HT,
    S_http_HTT,
    S_http_HTTP,
    S_first_major_digit,
    S_major_digit,
    S_first_minor_digit,
    S_minor_digit,
    S_spaces_after_digit,
    S_almost_done
};

static inline enum http_method get_request_method(str_t *method_name)
{
    unsigned char *p = method_name->p;

    switch (method_name->len) {
    case 3:
        if (CMP3(p, 'G', 'E', 'T'))
            return HTTP_GET;
        if (CMP3(p, 'P', 'U', 'T'))
            return HTTP_PUT;
        return HTTP_UNKNOWN;
    case 4:
        if (p[1] == 'O') {
            if (CMP30(p, 'P', 'O', 'S', 'T'))
                return HTTP_POST;
            if (CMP30(p, 'C', 'O', 'P', 'Y'))
                return HTTP_COPY;
            if (CMP30(p, 'M', 'O', 'V', 'E'))
                return HTTP_MOVE;
            if (CMP30(p, 'L', 'O', 'C', 'K'))
                return HTTP_LOCK;
            return HTTP_UNKNOWN;
        }
        if (CMP4(p, 'H', 'E', 'A', 'D'))
            return HTTP_HEAD;
        return HTTP_UNKNOWN;
    case 5:
        if (CMP5(p, 'M', 'K', 'C', 'O', 'L'))
            return HTTP_MKCOL;
        if (CMP5(p, 'P', 'A', 'T', 'C', 'H'))
            return HTTP_PATCH;
        if (CMP5(p, 'T', 'R', 'A', 'C', 'E'))
            return HTTP_TRACE;
        return HTTP_UNKNOWN;
    case 6:
        if (CMP6(p, 'D', 'E', 'L', 'E', 'T', 'E'))
            return HTTP_DELETE;
        if (CMP6(p, 'U', 'N', 'L', 'O', 'C', 'K'))
            return HTTP_UNLOCK;
        return HTTP_UNKNOWN;
    case 7:
        if (CMP7(p, 'O', 'P', 'T', 'I', 'O', 'N', 'S', ' '))
            return HTTP_OPTIONS;
        return HTTP_UNKNOWN;
    case 8:
        if (CMP8(p, 'P', 'R', 'O', 'P', 'F', 'I', 'N', 'D'))
            return HTTP_PROPFIND;
        return HTTP_UNKNOWN;
    case 9:
        if (CMP9(p, 'P', 'R', 'O', 'P', 'P', 'A', 'T', 'C', 'H'))
            return HTTP_PROPPATCH;
        return HTTP_UNKNOWN;
    }

    return HTTP_UNKNOWN;
}

/* Return 0 if complete; < 0 on error; > 0 continue */
int http_parse_request_line(struct http_request *r, struct buffer *b)
{
    unsigned char *p;
    int state = r->state;

    for (p = b->pos; p != b->last; p++) {
        unsigned char ch = *p;
        switch (state) {
        case S_start:
            r->request_line.p = p;
            if (ch == CR || ch == LF)
                break;
            if ((ch < 'A' || ch > 'Z') && ch != '_')
                return -1;
            r->method_name.p = p;
            state = S_method;
            break;

        case S_method:
            if (ch == ' ') {
                r->method_name.len = p - r->method_name.p;
                r->method = get_request_method(&r->method_name);
                if (HTTP_UNKNOWN & r->method)
                    return -2;
                state = S_spaces_before_uri;
                break;
            }

            if ((ch < 'A' || ch > 'Z') && ch != '_')
                return -3;
            break;

        case S_spaces_before_uri:
            if (ch == '/') {
                r->uri.p = p;
                str_init(&r->schema); /* no schema host/port, maybe in host */
                str_init(&r->host);
                str_init(&r->port);
                state = S_after_slash_in_uri;
                break;
            }

            unsigned char c = (unsigned char) (ch | 0x20);
            if (c >= 'a' && c <= 'z') {
                r->schema.p = p;
                state = S_schema;
                break;
            }
            if (ch == ' ')
                break;
            return -4;
        case S_schema:
            c = (unsigned char) (ch | 0x20);
            if (c >= 'a' && c <= 'z')
                break;
            if (ch == ':') {
                r->schema.len = p - r->schema.p;
                state = S_schema_slash;
                break;
            }
            return -5;
        case S_schema_slash:
            if (ch == '/') {
                state = S_schema_slash_slash;
                break;
            }
            return -6;
        case S_schema_slash_slash:
            if (ch == '/') {
                state = S_host_start;
                break;
            }
            return -7;
        case S_host_start:
            r->host.p = p;
            if (ch == '[') {
                state = S_host_ip_literal;
                break;
            }
            state = S_host;
            fallthrough;
        case S_host:
            c = (unsigned char) (ch | 0x20);
            if (c >= 'a' && c <= 'z')
                break;
            if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
                break;
            fallthrough;
        case S_host_end:
            r->host.len = p - r->host.p;
            switch (ch) {
            case ':':
                r->port.p = p + 1;
                state = S_port;
                break;
            case '/':
                str_init(&r->port);
                r->uri.p = p;
                state = S_after_slash_in_uri;
                break;
            case ' ':
                str_init(&r->port);
                r->uri.p = r->schema.p + r->schema.len + 1;
                r->uri.len = 1;
                state = S_host_http_09;
                break;
            default:
                return -8;
            }
            break;
        case S_host_ip_literal:
            if (ch >= '0' && ch <= '9')
                break;
            c = (unsigned char) (ch | 0x20);
            if (c >= 'a' && c <= 'z')
                break;
            switch (ch) {
            case ':':
                break;
            case ']':
                state = S_host_end;
                break;
            case '-':
            case '.':
            case '_':
            case '~':
                /* non-reserved */
                break;
            case '!':
            case '$':
            case '&':
            case '\'':
            case '(':
            case ')':
            case '*':
            case '+':
            case ',':
            case ';':
            case '=':
                /* sub-delims */
                break;
            default:
                return -9;
            }
            break;
        case S_port:
            if (ch >= '0' && ch <= '9')
                break;
            r->port.len = p - r->port.p;
            switch (ch) {
            case '/':
                r->uri.p = p;
                state = S_after_slash_in_uri;
                break;
            case ' ':
                /* GET http://host:port HTTP1.1 */
                r->uri.p = r->schema.p + r->schema.len + 1;
                r->uri.len = 1;
                state = S_host_http_09;
                break;
            default:
                return -10;
            }
            break;
        /* space+ after "http://host[:port] " */
        case S_host_http_09:
            switch (ch) {
            case ' ':
                break;
            case CR:
                r->http_minor = 9;
                state = S_almost_done;
                break;
            case LF:
                r->http_minor = 9;
                r->request_line.len = p - r->request_line.p;
                goto done;
            case 'H':
                r->http_protocol.p = p;
                state = S_http_H;
                break;
            default:
                return -11;
            }
            break;
            /* check the character following by "/" */
        case S_after_slash_in_uri:
            if (usual[ch >> 5] & (1 << (ch & 0x1f))) {
                state = S_check_uri;
                break;
            }
            switch (ch) {
            case ' ': /* check if version 0.9 */
                r->uri.len = p - r->uri.p;
                state = S_check_uri_http_09;
                break;
            case CR: /* assume version 0.9 */
                r->uri.len = p - r->uri.p;
                r->http_minor = 9;
                state = S_almost_done;
                break;
            case LF: /* assume version 0.9 */
                r->uri.len = p - r->uri.p;
                r->http_minor = 9;
                r->request_line.len = p - r->request_line.p;
                goto done;
            case '.':
                r->complex_uri = 1;
                state = S_uri;
                break;
            case '%':
                r->quoted_uri = 1;
                state = S_uri;
                break;
            case '/':
                r->complex_uri = 1;
                state = S_uri;
                break;
            case '?':
                r->args = p + 1;
                state = S_uri;
                break;
            case '#':
                r->complex_uri = 1;
                state = S_uri;
                break;
            case '+':
                r->plus_in_uri = 1;
                break;
            case '\0':
                return -12;
            default:
                state = S_check_uri;
                break;
            }
            break;
        /* check "/", "%" and "\" (Win32) in URI */
        case S_check_uri:
            if (usual[ch >> 5] & (1 << (ch & 0x1f)))
                break;
            switch (ch) {
            case '/':
                str_init(&r->exten);
                state = S_after_slash_in_uri;
                break;
            case '.':
                r->exten.p = p + 1;
                break;
            case ' ':
                r->uri.len = p - r->uri.p;
                state = S_check_uri_http_09;
                break;
            case CR:
                r->uri.len = p - r->uri.p;
                r->http_minor = 9;

                state = S_almost_done;
                break;
            case LF:
                r->uri.len = p - r->uri.p;
                r->http_minor = 9;
                r->request_line.len = p - r->request_line.p;
                goto done;
            case '%':
                r->quoted_uri = 1;
                state = S_uri;
                break;
            case '?':
                r->args = p + 1;
                state = S_uri;
                break;
            case '#':
                r->complex_uri = 1;
                state = S_uri;
                break;
            case '+':
                r->plus_in_uri = 1;
                break;
            case '\0':
                return -13;
            }
            break;
        case S_check_uri_http_09:
            switch (ch) {
            case ' ':
                break;
            case CR:
                r->http_minor = 9;
                state = S_almost_done;
                break;
            case LF:
                r->http_minor = 9;
                r->request_line.len = p - r->request_line.p;
                goto done;
            case 'H':
                r->http_protocol.p = p;
                state = S_http_H;
                break;
            default:
                r->space_in_uri = 1;
                state = S_check_uri;
                p--;
                break;
            }
            break;
        /* URI */
        case S_uri: /* the character following (%?#/). regard as uri */
            if (usual[ch >> 5] & (1 << (ch & 0x1f)))
                break;
            switch (ch) {
            case ' ':
                r->uri.len = p - r->uri.p;
                state = S_http_09;
                break;
            case CR:
                r->uri.len = p - r->uri.p;
                r->http_minor = 9;
                state = S_almost_done;
                break;
            case LF:
                r->uri.len = p - r->uri.p;
                r->http_minor = 9;
                r->request_line.len = p - r->request_line.p;
                goto done;
            case '#':
                r->complex_uri = 1;
                break;
            case '\0':
                return -14;
            }
            break;
        /* space+ after URI */
        case S_http_09:
            switch (ch) {
            case ' ':
                break;
            case CR:
                r->http_minor = 9;
                state = S_almost_done;
                break;
            case LF:
                r->http_minor = 9;
                r->request_line.len = p - r->request_line.p;
                goto done;
            case 'H':
                r->http_protocol.p = p;
                state = S_http_H;
                break;
            default:
                r->space_in_uri = 1;
                state = S_uri;
                p--;
                break;
            }
            break;
        case S_http_H:
            switch (ch) {
            case 'T':
                state = S_http_HT;
                break;
            default:
                return -15;
            }
            break;
        case S_http_HT:
            switch (ch) {
            case 'T':
                state = S_http_HTT;
                break;
            default:
                return -16;
            }
            break;
        case S_http_HTT:
            switch (ch) {
            case 'P':
                state = S_http_HTTP;
                break;
            default:
                return -17;
            }
            break;
        case S_http_HTTP:
            switch (ch) {
            case '/':
                state = S_first_major_digit;
                break;
            default:
                return -18;
            }
            break;
        /* first digit of major HTTP version */
        case S_first_major_digit:
            if (ch < '1' || ch > '9')
                return -19;
            r->http_major = ch - '0';
            state = S_major_digit;
            break;
        /* major HTTP version or dot */
        case S_major_digit:
            if (ch == '.') {
                state = S_first_minor_digit;
                break;
            }
            if (ch < '0' || ch > '9')
                return -20;
            r->http_major = r->http_major * 10 + ch - '0';
            break;
        /* first digit of minor HTTP version */
        case S_first_minor_digit:
            if (ch < '0' || ch > '9')
                return -21;
            r->http_minor = ch - '0';
            state = S_minor_digit;
            break;
        /* minor HTTP version or end of r line */
        case S_minor_digit:
            if (ch == CR) {
                r->http_protocol.len = p - r->http_protocol.p;
                state = S_almost_done;
                break;
            }

            if (ch == LF) {
                r->request_line.len = p - r->request_line.p;
                r->http_protocol.len = p - r->http_protocol.p;
                goto done;
            }

            if (ch == ' ') {
                r->http_protocol.len = p - r->http_protocol.p;
                state = S_spaces_after_digit;
                break;
            }

            if (ch < '0' || ch > '9')
                return -22;
            r->http_minor = r->http_minor * 10 + ch - '0';
            break;
        case S_spaces_after_digit:
            switch (ch) {
            case ' ':
                break;
            case CR:
                state = S_almost_done;
                break;
            case LF:
                r->request_line.len = p - r->request_line.p;
                goto done;
            default:
                return -23;
            }
            break;
        /* end of r line */
        case S_almost_done:
            r->request_line.len = p - r->request_line.p - 1;
            switch (ch) {
            case LF:
                goto done;
            default:
                return -24;
            }
        }
    }

    b->pos = p;
    r->state = state;

    return 1;
done:
    if (r->exten.p)
        r->exten.len = r->uri.p + r->uri.len - r->exten.p;
    b->pos = p + 1;
    if (r->http_minor == 9)
        r->http_major = 0;
    r->http_version = r->http_major * 1000 + r->http_minor;
    r->state = S_start;
    if (r->http_version == 9 && r->method != HTTP_GET)
        return -25;

    return 0;
}

/* Return 0 on complete; 1: continue; 100: done with one line; < 0 on error */
int http_parse_request_header(struct http_request *r, struct buffer *b)
{
    enum {
        S_start = 0,
        S_name,
        S_space_before_value,
        S_value,
        S_space_after_value,
        /* S_ignore_line, */
        S_almost_done,
        S_header_almost_done
    } state;

    static unsigned char lowcase[] =
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0"
        "0123456789\0\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

    unsigned char c, ch, *p;
    state = r->state;
    unsigned hash = r->header_hash;
    unsigned i = r->lowcase_index;

    for (p = b->pos; p < b->last; p++) {
        ch = *p;
        switch (state) {
        case S_start:
            r->header_name.p = p;
            r->invalid_header = 0;
            switch (ch) {
            case CR:
                r->header_name.len = p - r->header_name.p;
                state = S_header_almost_done;
                break;
            case LF:
                r->header_name.len = p - r->header_name.p;
                goto header_done;
            default:
                state = S_name;
                c = lowcase[ch];
                if (c) {
                    hash = HASH(0, c);
                    r->lowcase_header[0] = c;
                    i = 1;
                    break;
                }

                if (ch == '\0')
                    return -1;
                r->invalid_header = 1;
                break;
            }
            break;
        /* header name */
        case S_name:
            c = lowcase[ch]; /* Only handle 0-9, -, A-Z, a-z */
            if (c) {
                hash = HASH(hash, c);
                r->lowcase_header[i++] = c;
                i &= (HTTP_LC_HEADER_LEN - 1);
                break;
            }
            if (ch == ':') {
                r->header_name.len = p - r->header_name.p;
                state = S_space_before_value;
                break;
            }
            if (ch == CR) {
                r->header_name.len = p - r->header_name.p;
                str_init(&r->header_value);
                state = S_almost_done;
                break;
            }
            if (ch == LF) {
                r->header_name.len = p - r->header_name.p;
                str_init(&r->header_value);
                goto done;
            }
            if (ch == '\0')
                return -2;
            r->invalid_header = 1;
            break;
        /* space* before header value */
        case S_space_before_value:
            switch (ch) {
            case ' ':
                break;
            case CR:
                str_init(&r->header_value);
                state = S_almost_done;
                break;
            case LF:
                str_init(&r->header_value);
                goto done;
            case '\0':
                return -3;
            default:
                r->header_value.p = p;
                state = S_value;
                break;
            }
            break;
        /* header value */
        case S_value:
            switch (ch) {
            case ' ':
                r->header_value.len = p - r->header_value.p;
                state = S_space_after_value;
                break;
            case CR:
                r->header_value.len = p - r->header_value.p;
                state = S_almost_done;
                break;
            case LF:
                r->header_value.len = p - r->header_value.p;
                goto done;
            case '\0':
                return -4;
            }
            break;
        /* space* before end of header line */
        case S_space_after_value:
            switch (ch) {
            case ' ':
                break;
            case CR:
                state = S_almost_done;
                break;
            case LF:
                goto done;
            case '\0':
                return -5;
            default:
                state = S_value;
                break;
            }
            break;
        /* end of header line */
        case S_almost_done:
            switch (ch) {
            case LF:
                goto done;
            case CR:
                break;
            default:
                return -6;
            }
            break;
        /* end of header */
        case S_header_almost_done:
            switch (ch) {
            case LF:
                goto header_done;
            default:
                return -7;
            }
        }
    }
    b->pos = p;
    r->state = state;
    r->header_hash = hash;
    r->lowcase_index = i;
    return 1; /* continue */

done:
    b->pos = p + 1;
    r->state = S_start;
    r->header_hash = hash;
    r->lowcase_index = i;
    return 0; /* request for next line */

header_done:
    b->pos = p + 1;
    r->state = S_start;
    return 100; /* all requests are completed */
}
