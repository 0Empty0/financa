#include "event_loop.h"
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#define INITIAL_EVENT_CAPACITY 64

typedef struct event
{
    int fd;
    short events;
    void *data;
    event_callback_t callback;
} event_t;

struct event_loop
{
    event_t *events;
    nfds_t count;
    nfds_t capacity;
};

int event_loop_create(event_loop_t **loop)
{
    *loop = malloc(sizeof(event_loop_t));

    if (*loop == NULL)
        return -1;

    (*loop)->events = malloc(sizeof(event_t) * INITIAL_EVENT_CAPACITY);

    if ((*loop)->events == NULL)
    {
        free(*loop);
        return -1;
    }

    (*loop)->count = 0;
    (*loop)->capacity = INITIAL_EVENT_CAPACITY;

    return 0;
}

void event_loop_destroy(event_loop_t *loop)
{
    if (loop == NULL)
        return;

    free(loop->events);
    free(loop);
}

int event_loop_add(event_loop_t *loop, int fd, short events, void *data, event_callback_t callback)
{
    for (nfds_t i = 0; i < loop->count; i++)
    {
        if (loop->events[i].fd == fd)
        {
            loop->events[i].events = events;
            loop->events[i].data = data;
            loop->events[i].callback = callback;
        }
    }

    if (loop->count == loop->capacity)
    {
        nfds_t new_capacity = loop->capacity * 2;

        event_t *new_events = realloc(loop->events, sizeof(event_t) * loop->capacity);

        if (new_events == NULL)
            return -1;

        loop->capacity = new_capacity;
        loop->events = new_events;
    }

    loop->events[loop->count].fd = fd;
    loop->events[loop->count].events = events;
    loop->events[loop->count].data = data;
    loop->events[loop->count].callback = callback;
    loop->count++;

    return 0;
}

int event_loop_remove(event_loop_t *loop, int fd)
{
    for (nfds_t i = 0; i < loop->count; i++)
    {
        if (loop->events[i].fd == fd)
        {
            loop->events[i] = loop->events[loop->count - 1];
            loop->count--;
            return 0;
        }
    }

    return -1;
}

void event_loop_run(event_loop_t *loop)
{
    while (true)
    {
        struct pollfd *pfds = malloc(sizeof(struct pollfd) * loop->count);
        if (pfds == NULL)
            break;

        for (nfds_t i = 0; i < loop->count; i++)
        {
            pfds[i].fd = loop->events[i].fd;
            pfds[i].events = loop->events[i].events;
            pfds[i].revents = 0;
        }

        int timeout = 1000;
        int ret = poll(pfds, loop->count, timeout);

        if (ret < 0)
        {
            perror("poll");
            free(pfds);
            continue;
        }

        for (nfds_t i = 0; i < loop->count; i++)
        {
            if (!pfds[i].revents)
                continue;

            if (!loop->events[i].callback)
                continue;

            loop->events[i].callback(loop->events[i].fd, pfds[i].revents, loop->events[i].data);
        }

        free(pfds);
    }
}
