#include <stdlib.h>

#include "http/request.h"
#include "internal.h"
#include "logger.h"
#include "process.h"
#include "util/conf.h"

static int worker_process_init()
{
    size_t size = 2;
    char *c = get_conf_entry("client_header_buffer_kbytes");
    if (!str_equal(c, "default")) {
        size = atoi(c);
        if (size == 0 || size > 4) {
            ERR("client header buffer size should between [1-4]KiB");
            return -1;
        }
    }

    http_request_init(size, NULL, NULL, NULL);
    return 0;
}

__INIT static void http_init()
{
    register_service(NULL, worker_process_init, http_request_handler);
}
