#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define INITIAL_ROUTE_CAPACITY 64
typedef struct route
{
	char *path;
	void *data;
	http_route_callback_t callback;
} route_t;

struct http_server
{
	event_loop_t *loop;
	int listen_fd;
	int port;
	route_t *routes;
	size_t route_count;
	size_t route_capacity;
};

static int set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void http_client_callback(int fd, short events, void *data);

static void http_accept_callback(int fd, short events, void *data)
{
	http_server_t *server = data;
	while (1)
	{
		struct sockaddr_in client_addr;
		socklen_t addrlen = sizeof(client_addr);
		int client_fd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
		if (client_fd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			perror("accept");
			break;
		}

		set_nonblocking(client_fd);

		if (event_loop_add(server->loop, client_fd, POLLIN, server, http_client_callback) < 0)
		{
			perror("event_loop_add");
			close(client_fd);
		}
	}
}

static void http_client_callback(int fd, short events, void *data)
{
	http_server_t *server = (http_server_t *)data;
	char buffer[4096];
	ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
	if (n <= 0)
	{
		if (n < 0)
			perror("read");
		event_loop_remove(server->loop, fd);
		close(fd);
		return;
	}
	buffer[n] = '\0';
	printf("Received request:\n%s\n", buffer);

	char method[256], path[8192], protocol[256];
	if (sscanf(buffer, "%255s %8191s %255s", method, path, protocol) != 3)
	{
		const char *bad_response =
				"HTTP/1.1 400 Bad Request\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 11\r\n"
				"\r\n"
				"Bad Request";

		write(fd, bad_response, strlen(bad_response));
		event_loop_remove(server->loop, fd);
		close(fd);
		return;
	}

	for (size_t i = 0; i < server->route_count; i++)
	{
		if (strcmp(path, server->routes[i].path) == 0)
		{
			// Note: the callback is expected to handle writing the response
			// and closing the connection.
			// TODO: make callback receive request and response, so this func will handle writing
			server->routes[i].callback(fd, buffer, server->routes[i].data);
			return;
		}
	}

	const char *not_found_response =
			"HTTP/1.1 404 Not Found\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 13\r\n"
			"\r\n"
			"404 Not Found";

	write(fd, not_found_response, strlen(not_found_response));
	event_loop_remove(server->loop, fd);
	close(fd);
}

http_server_t *http_server_create(event_loop_t *loop, int port)
{
	http_server_t *server = malloc(sizeof(http_server_t));
	if (!server)
		return NULL;
	server->loop = loop;
	server->port = port;
	server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server->listen_fd < 0)
	{
		perror("socket");
		free(server);
		return NULL;
	}

	int opt = 1;
	if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		close(server->listen_fd);
		free(server);
		return NULL;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		close(server->listen_fd);
		free(server);
		return NULL;
	}

	if (listen(server->listen_fd, SOMAXCONN) < 0)
	{
		perror("listen");
		close(server->listen_fd);
		free(server);
		return NULL;
	}

	set_nonblocking(server->listen_fd);

	if (event_loop_add(server->loop, server->listen_fd, POLLIN, server, http_accept_callback) < 0)
	{
		perror("event_loop_add");
		close(server->listen_fd);
		free(server);
		return NULL;
	}

	printf("HTTP server listening on port %d\n", port);

	server->route_count = 0;
	server->route_capacity = INITIAL_ROUTE_CAPACITY;
	server->routes = malloc(sizeof(struct route) * server->route_capacity);

	if (server->routes == NULL)
	{
		perror("routes");
		close(server->listen_fd);
		free(server);
		return NULL;
	}

	return server;
}

void http_server_destroy(http_server_t *server)
{
	if (server == NULL)
	{
		return;
	}

	event_loop_remove(server->loop, server->listen_fd);
	close(server->listen_fd);
	for (size_t i = 0; i < server->route_count; i++)
	{
		free(server->routes[i].path);
	}
	free(server->routes);
	free(server);
}

int http_server_add_route(http_server_t *server, const char *path,
													void *data, http_route_callback_t callback)
{
	if (server == NULL || path == NULL || callback == NULL)
	{
		return -1;
	}

	if (server->route_count == server->route_capacity)
	{
		size_t new_capacity = server->route_capacity * 2;

		route_t *new_routes = realloc(server->routes, sizeof(route_t) * new_capacity);

		if (new_routes == NULL)
		{
			perror("realloc");
			return -1;
		}

		server->routes = new_routes;
		server->route_capacity = new_capacity;
	}

	server->routes[server->route_count].path = strdup(path);

	if (server->routes[server->route_count].path == NULL)
	{
		perror("path");
		return -1;
	}

	server->routes[server->route_count].data = data;
	server->routes[server->route_count].callback = callback;
	server->route_count++;

	return 0;
}
