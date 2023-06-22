#include <sys/socket.h>

#include "http/request.h"
#include "http/response.h"
#include "logger.h"
#include "util/str.h"

static str_t http_status_lines[] = {
    STRING("200 OK"),
    STRING("201 Created"),
    STRING("202 Accepted"),
    NULL_STRING, /* "203 Non-Authoritative Information" */
    STRING("204 No Content"),
    NULL_STRING, /* "205 Reset Content" */
    STRING("206 Partial Content"),
#define HTTP_STATUS_LAST_2XX 207
#define HTTP_STATUS_OFFSET_3XX (HTTP_STATUS_LAST_2XX - 200)
    STRING("301 Moved Permanently"),
    STRING("302 Moved Temporarily"),
    STRING("303 See Other"),
    STRING("304 Not Modified"),
    NULL_STRING, /* "305 Use Proxy" */
    NULL_STRING, /* "306 unused" */
    STRING("307 Temporary Redirect"),
#define HTTP_STATUS_LAST_3XX 308
#define HTTP_STATUS_OFFSET_4XX \
    (HTTP_STATUS_LAST_3XX - 301 + HTTP_STATUS_OFFSET_3XX)
    STRING("400 Bad Request"),
    STRING("401 Unauthorized"),
    STRING("402 Payment Required"),
    STRING("403 Forbidden"),
    STRING("404 Not Found"),
    STRING("405 Not Allowed"),
    STRING("406 Not Acceptable"),
    NULL_STRING, /* "407 Proxy Authentication Required" */
    STRING("408 Request Time-out"),
    STRING("409 Conflict"),
    STRING("410 Gone"),
    STRING("411 Length Required"),
    STRING("412 Precondition Failed"),
    STRING("413 Request Entity Too Large"),
    STRING("414 Request-URI Too Large"),
    STRING("415 Unsupported Media Type"),
    STRING("416 Requested Range Not Satisfiable"),
#define HTTP_STATUS_LAST_4XX 417
#define HTTP_STATUS_OFFSET_5XX \
    (HTTP_STATUS_LAST_4XX - 400 + HTTP_STATUS_OFFSET_4XX)
    STRING("500 Internal Server Error"),
    STRING("501 Not Implemented"),
    STRING("502 Bad Gateway"),
    STRING("503 Service Temporarily Unavailable"),
    STRING("504 Gateway Time-out"),

    NULL_STRING, /* "505 HTTP Version Not Supported" */
    NULL_STRING, /* "506 Variant Also Negotiates" */
    STRING("507 Insufficient Storage"),
#define HTTP_STATUS_OFFSET_LAST 508
};

static char http_error_200_page[] =
    "<html>" CRLF "<head><title>cserv</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>welcome to cserv</h1></center>" CRLF;

static char http_error_301_page[] =
    "<html>" CRLF "<head><title>301 Moved Permanently</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>301 Moved Permanently</h1></center>" CRLF;

static char http_error_302_page[] =
    "<html>" CRLF "<head><title>302 Found</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF "<center><h1>302 Found</h1></center>" CRLF;

static char http_error_303_page[] =
    "<html>" CRLF "<head><title>303 See Other</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>303 See Other</h1></center>" CRLF;

static char http_error_307_page[] =
    "<html>" CRLF "<head><title>307 Temporary Redirect</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>307 Temporary Redirect</h1></center>" CRLF;

static char http_error_400_page[] =
    "<html>" CRLF "<head><title>400 Bad Request</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF;

static char http_error_401_page[] =
    "<html>" CRLF "<head><title>401 Authorization Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>401 Authorization Required</h1></center>" CRLF;

static char http_error_402_page[] =
    "<html>" CRLF "<head><title>402 Payment Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>402 Payment Required</h1></center>" CRLF;

static char http_error_403_page[] =
    "<html>" CRLF "<head><title>403 Forbidden</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>403 Forbidden</h1></center>" CRLF;

static char http_error_404_page[] =
    "<html>" CRLF "<head><title>404 Not Found</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>404 Not Found</h1></center>" CRLF;

static char http_error_405_page[] =
    "<html>" CRLF "<head><title>405 Not Allowed</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>405 Not Allowed</h1></center>" CRLF;

static char http_error_406_page[] =
    "<html>" CRLF "<head><title>406 Not Acceptable</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>406 Not Acceptable</h1></center>" CRLF;

static char http_error_408_page[] =
    "<html>" CRLF "<head><title>408 Request Time-out</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>408 Request Time-out</h1></center>" CRLF;

static char http_error_409_page[] =
    "<html>" CRLF "<head><title>409 Conflict</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>409 Conflict</h1></center>" CRLF;

static char http_error_410_page[] =
    "<html>" CRLF "<head><title>410 Gone</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF "<center><h1>410 Gone</h1></center>" CRLF;

static char http_error_411_page[] =
    "<html>" CRLF "<head><title>411 Length Required</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>411 Length Required</h1></center>" CRLF;

static char http_error_412_page[] =
    "<html>" CRLF "<head><title>412 Precondition Failed</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>412 Precondition Failed</h1></center>" CRLF;

static char http_error_413_page[] =
    "<html>" CRLF
    "<head><title>413 Request Entity Too Large</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>413 Request Entity Too Large</h1></center>" CRLF;

static char http_error_414_page[] =
    "<html>" CRLF "<head><title>414 Request-URI Too Large</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>414 Request-URI Too Large</h1></center>" CRLF;

static char http_error_415_page[] =
    "<html>" CRLF "<head><title>415 Unsupported Media Type</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>415 Unsupported Media Type</h1></center>" CRLF;

static char http_error_416_page[] =
    "<html>" CRLF
    "<head><title>416 Requested Range Not Satisfiable</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>416 Requested Range Not Satisfiable</h1></center>" CRLF;

static char http_error_494_page[] =
    "<html>" CRLF
    "<head><title>400 Request Header Or Cookie Too Large</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>Request Header Or Cookie Too Large</center>" CRLF;

static char http_error_495_page[] =
    "<html>" CRLF
    "<head><title>400 The SSL certificate error</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>The SSL certificate error</center>" CRLF;

static char http_error_496_page[] =
    "<html>" CRLF
    "<head><title>400 No required SSL certificate was sent</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>No required SSL certificate was sent</center>" CRLF;

static char http_error_497_page[] =
    "<html>" CRLF
    "<head><title>400 The plain HTTP request was sent to HTTPS "
    "port</title></head>" CRLF "<body bgcolor=\"white\">" CRLF
    "<center><h1>400 Bad Request</h1></center>" CRLF
    "<center>The plain HTTP request was sent to HTTPS port</center>" CRLF;

static char http_error_500_page[] =
    "<html>" CRLF "<head><title>500 Internal Server Error</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>500 Internal Server Error</h1></center>" CRLF;

static char http_error_501_page[] =
    "<html>" CRLF "<head><title>501 Not Implemented</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>501 Not Implemented</h1></center>" CRLF;

static char http_error_502_page[] =
    "<html>" CRLF "<head><title>502 Bad Gateway</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>502 Bad Gateway</h1></center>" CRLF;

static char http_error_503_page[] =
    "<html>" CRLF
    "<head><title>503 Service Temporarily Unavailable</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>503 Service Temporarily Unavailable</h1></center>" CRLF;

static char http_error_504_page[] =
    "<html>" CRLF "<head><title>504 Gateway Time-out</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>504 Gateway Time-out</h1></center>" CRLF;

static char http_error_507_page[] =
    "<html>" CRLF "<head><title>507 Insufficient Storage</title></head>" CRLF
    "<body bgcolor=\"white\">" CRLF
    "<center><h1>507 Insufficient Storage</h1></center>" CRLF;

static str_t http_error_pages[] = {
    STRING(http_error_301_page),
    STRING(http_error_302_page),
    STRING(http_error_303_page),
    NULL_STRING, /* 304 */
    NULL_STRING, /* 305 */
    NULL_STRING, /* 306 */
    STRING(http_error_307_page),
#define HTTP_ERROR_LAST_3XX 308
#define HTTP_ERROR_OFFSET_4XX (HTTP_ERROR_LAST_3XX - 301)
    STRING(http_error_400_page),
    STRING(http_error_401_page),
    STRING(http_error_402_page),
    STRING(http_error_403_page),
    STRING(http_error_404_page),
    STRING(http_error_405_page),
    STRING(http_error_406_page),
    NULL_STRING, /* 407 */
    STRING(http_error_408_page),
    STRING(http_error_409_page),
    STRING(http_error_410_page),
    STRING(http_error_411_page),
    STRING(http_error_412_page),
    STRING(http_error_413_page),
    STRING(http_error_414_page),
    STRING(http_error_415_page),
    STRING(http_error_416_page),
#define HTTP_ERROR_LAST_4XX 417
#define HTTP_ERROR_OFFSET_494 \
    (HTTP_ERROR_LAST_4XX - 400 + HTTP_ERROR_OFFSET_4XX)
    STRING(http_error_494_page), /* 494, request header too large */
    STRING(http_error_495_page), /* 495, https certificate error */
    STRING(http_error_496_page), /* 496, https no certificate */
    STRING(http_error_497_page), /* 497, http to https */
    STRING(http_error_404_page), /* 498, canceled */
    NULL_STRING,                 /* 499, client has closed connection */

    STRING(http_error_500_page),
    STRING(http_error_501_page),
    STRING(http_error_502_page),
    STRING(http_error_503_page),
    STRING(http_error_504_page),
    NULL_STRING, /* 505 */
    NULL_STRING, /* 506 */
    STRING(http_error_507_page)

#define HTTP_ERROR_OFFSET_LAST 508
};

static inline str_t *get_http_status_line(int ret_code)
{
    str_t *status_line;

    if (ret_code >= HTTP_OK && ret_code < HTTP_STATUS_LAST_2XX)
        status_line = &http_status_lines[ret_code - HTTP_OK];
    else if (ret_code >= HTTP_MOVED_PERMANENTLY &&
             ret_code < HTTP_STATUS_LAST_3XX)
        status_line = &http_status_lines[ret_code - HTTP_MOVED_PERMANENTLY +
                                         HTTP_STATUS_OFFSET_3XX];
    else if (ret_code >= HTTP_BAD_REQUEST && ret_code < HTTP_STATUS_LAST_4XX)
        status_line = &http_status_lines[ret_code - HTTP_BAD_REQUEST +
                                         HTTP_STATUS_OFFSET_4XX];
    else if (ret_code >= HTTP_INTERNAL_SERVER_ERROR &&
             ret_code < HTTP_STATUS_OFFSET_LAST)
        status_line = &http_status_lines[ret_code - HTTP_INTERNAL_SERVER_ERROR +
                                         HTTP_STATUS_OFFSET_5XX];
    else {
        CRIT("unknown status code. %d", ret_code);
        return NULL;
    }

    return likely(status_line->p) ? status_line : NULL;
}

static inline str_t *get_http_error_page(int ret_code)
{
    str_t *error_page;

    if (ret_code >= HTTP_MOVED_PERMANENTLY && ret_code < HTTP_ERROR_LAST_3XX)
        error_page = &http_error_pages[ret_code - HTTP_MOVED_PERMANENTLY];
    else if (ret_code >= HTTP_BAD_REQUEST && ret_code < HTTP_ERROR_LAST_4XX)
        error_page = &http_error_pages[ret_code - HTTP_BAD_REQUEST +
                                       HTTP_ERROR_OFFSET_4XX];
    else if (ret_code >= 494 && ret_code < HTTP_ERROR_OFFSET_LAST)
        error_page = &http_error_pages[ret_code - 494 + HTTP_ERROR_OFFSET_494];
    else
        return NULL;

    return error_page;
}

void http_fast_response(int fd, const char *content, size_t len)
{
    send(fd, content, len, 0);
}

void http_finalize_request(struct http_request *r, int ret_code)
{
    struct buffer *b = &r->header;
    str_t *status_line = get_http_status_line(ret_code);
    if (!status_line)
        return;

    b->pos = b->last = b->start;
    memcpy(b->start, "HTTP/1.1 ", sizeof("HTTP/1.1 ") - 1);
    b->pos += sizeof("HTTP/1.1 ") - 1;

    memcpy(b->pos, status_line->p, status_line->len);
    b->pos += status_line->len;

    memcpy(b->pos, CRLF HTTP_ERROR_HEADER, 2 + sizeof(HTTP_ERROR_HEADER) - 1);
    b->pos += 2 + sizeof(HTTP_ERROR_HEADER) - 1;

    if (ret_code == 200) {
        memcpy(b->pos, CRLF, 2);
        b->pos += 2;
        memcpy(b->pos, http_error_200_page, sizeof(http_error_200_page) - 1);
        b->pos += sizeof(http_error_200_page) - 1;
    } else {
        str_t *error_page = get_http_error_page(ret_code);
        if (error_page) {
            memcpy(b->pos, CRLF, 2);
            b->pos += 2;
            memcpy(b->pos, error_page->p, error_page->len);
            b->pos += error_page->len;
        }
    }

    send(r->fd, b->start, b->pos - b->start, 0);
}
