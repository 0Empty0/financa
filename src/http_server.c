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

struct http_server
{
	event_loop_t *loop;
	int listen_fd;
	int port;
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

	const char *response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 13\r\n"
			"\r\n"
			"Hello, World!";
	ssize_t total_written = 0;
	ssize_t response_len = strlen(response);
	while (total_written < response_len)
	{
		ssize_t written = write(fd, response + total_written, response_len - total_written);
		if (written < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;
			perror("write");
			break;
		}
		total_written += written;
	}
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

	return server;
}

void http_server_destroy(http_server_t *server)
{
	if (server != NULL)
	{
		event_loop_remove(server->loop, server->listen_fd);
		close(server->listen_fd);
		free(server);
	}
}