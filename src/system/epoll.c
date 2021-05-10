
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/epoll.h>

#include "hin.h"

#define MAX_EVENTS 32
static int epoll_fd = -1;

static int hin_init_epoll () {
  if (epoll_fd < 0) {
    epoll_fd = epoll_create1 (0);
    if (epoll_fd < 0) {
      perror ("epoll_create");
      return -1;
    }
  }
  return 0;
}

int hin_epoll_request_read (hin_buffer_t * buf) {
  if (hin_init_epoll () < 0) {
    return -1;
  }

  struct epoll_event event;
  event.events = EPOLLIN | EPOLLONESHOT;
  event.data.ptr = buf;

  int op = EPOLL_CTL_MOD;
  if ((buf->flags & HIN_EPOLL_INIT) == 0) {
    op = EPOLL_CTL_ADD;
  }
  buf->flags |= HIN_EPOLL_INIT;

  if (epoll_ctl (epoll_fd, op, buf->fd, &event) < 0) {
    perror ("epoll_ctl");
    return -1;
  }

  return 0;
}

int hin_epoll_check () {
  if (epoll_fd < 0) return 0;

  struct epoll_event events[MAX_EVENTS];
  int event_count = epoll_wait (epoll_fd, events, MAX_EVENTS, 0);
  for (int i = 0; i < event_count; i++) {
    hin_buffer_t * buf = events[i].data.ptr;

    int ret = read (buf->fd, buf->ptr, buf->count);
    if (ret < 0) ret = -errno;

    int err = buf->callback (buf, ret);
    if (err) {
      hin_buffer_clean (buf);
    }
  }

  return 0;
}


