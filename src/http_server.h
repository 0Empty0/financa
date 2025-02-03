#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "event_loop.h"

typedef void (*http_route_callback_t)(int fd, const char *request, void *data);

typedef struct http_server http_server_t;

http_server_t *http_server_create(event_loop_t *loop, int port);

void http_server_destroy(http_server_t *server);

int http_server_add_route(http_server_t *server, const char *path,
													void *data, http_route_callback_t callback);

#endif // HTTP_SERVER_H