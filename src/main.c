#include "event_loop.h"
#include "http_server.h"

#include <stdio.h>

int main(void)
{
    event_loop_t *loop;

    if (event_loop_create(&loop) < 0)
    {
        fprintf(stderr, "Failed to create event loop\n");
        return -1;
    }

    int port = 8080;

    http_server_t *server = http_server_create(loop, port);
    if (server == NULL)
    {
        fprintf(stderr, "Failed to create HTTP server\n");
        event_loop_destroy(loop);
        return -1;
    }

    event_loop_run(loop);

    http_server_destroy(server);
    event_loop_destroy(loop);

    return 0;
}
