#ifndef HTTP_SERVER_H 
#define HTTP_SERVER_H

#include "event_loop.h"

typedef struct http_server http_server_t;

http_server_t *http_server_create(event_loop_t *loop, int port);

void http_server_destroy(http_server_t *server);

#endif // HTTP_SERVER_H