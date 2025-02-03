#include "event_loop.h"
#include "http_server.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

void example_route(int fd, const char *request, void *data) {
    (void)request;
    (void)data;

    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        "Hello World!";

    write(fd, response, strlen(response));
    close(fd);
}

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

    if (http_server_add_route(server, "/example", NULL, example_route)) {
        fprintf(stderr, "Failed to register /example route\n");
    }

    event_loop_run(loop);

    http_server_destroy(server);
    event_loop_destroy(loop);

    return 0;
}
