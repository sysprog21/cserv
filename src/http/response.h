#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "http/request.h"

#define HTTP_CONTINUE 100
#define HTTP_SWITCHING_PROTOCOLS 101
#define HTTP_PROCESSING 102

#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_ACCEPTED 202
#define HTTP_NO_CONTENT 204
#define HTTP_PARTIAL_CONTENT 206

#define HTTP_MOVED_PERMANENTLY 301
#define HTTP_MOVED_TEMPORARILY 302
#define HTTP_SEE_OTHER 303
#define HTTP_NOT_MODIFIED 304
#define HTTP_TEMPORARY_REDIRECT 307

#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_NOT_ALLOWED 405
#define HTTP_REQUEST_TIME_OUT 408
#define HTTP_CONFLICT 409
#define HTTP_LENGTH_REQUIRED 411
#define HTTP_PRECONDITION_FAILED 412
#define HTTP_REQUEST_ENTITY_TOO_LARGE 413
#define HTTP_REQUEST_URI_TOO_LARGE 414
#define HTTP_UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_RANGE_NOT_SATISFIABLE 416

#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_NOT_IMPLEMENTED 501
#define HTTP_BAD_GATEWAY 502
#define HTTP_SERVICE_UNAVAILABLE 503
#define HTTP_GATEWAY_TIME_OUT 504
#define HTTP_INSUFFICIENT_STORAGE 507

#define HTTP_ERROR_HEADER                                   \
    "Server: cserv/dev" CRLF "Content-Type: text/html" CRLF \
    "Connection: Close" CRLF

#define HTTP_INSUFFICIENT_STORAGE_PAGE                                        \
    "HTTP/1.1 507 Insufficient Storage" CRLF HTTP_ERROR_HEADER CRLF           \
    "<html>" CRLF "<head><title>507 Insufficient Storage</title></head>" CRLF \
    "<body bgcolor=\"white\">" CRLF                                           \
    "<center><h1>507 Insufficient Storage</h1></center>" CRLF

void http_fast_response(int fd, const char *content, size_t len);
void http_finalize_request(struct http_request *r, int ret_code);

#endif
