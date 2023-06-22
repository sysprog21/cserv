#pragma once

#include "http/request.h"

int http_parse_request_line(struct http_request *request, struct buffer *b);
int http_parse_request_header(struct http_request *request, struct buffer *b);
