#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <poll.h>

typedef struct event_loop event_loop_t;

typedef void (*event_callback_t)(int fd, short events, void *data);

int event_loop_create(event_loop_t **loop);

void event_loop_destroy(event_loop_t *loop);

int event_loop_add(event_loop_t *loop, int fd, short events, void *data, event_callback_t callback);

int event_loop_remove(event_loop_t *loop, int fd);

void event_loop_run(event_loop_t *loop);

#endif // EVENT_LOOP_H
