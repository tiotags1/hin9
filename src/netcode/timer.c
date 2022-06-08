
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include "hin.h"
#include "hin_internal.h"
#include "conf.h"

static hin_buffer_t * timeout_buffer = NULL;

void hin_timer_clean () {
  if (timeout_buffer)
    hin_buffer_stop_clean (timeout_buffer);
}

typedef struct {
  time_t time;
  struct __kernel_timespec ts;
  int pad;
  int (*callback) (int ms);
} hin_timeout_t;

static int hin_timer_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0 && ret != -ETIME) {
    hin_error ("timer error %s", strerror (-ret));
  }
  hin_timeout_t * tm = (void*)&buffer->buffer;
  if (hin_request_timeout (buffer, &tm->ts, 0, 0) < 0) {
    hin_weird_error (78347122);
    return -1;
  }

  int hin_epoll_check ();
  hin_epoll_check ();

  time_t new = time (NULL);
  if (tm->time == new) return 0;

  if (tm->callback)
    tm->callback (1000);

  tm->time = new;

  return 0;
}

int hin_timer_init (int (*callback) (int ms)) {
  if (timeout_buffer) {
    hin_timeout_t * tm = (void*)&timeout_buffer->buffer;
    tm->callback = callback;
    return 0;
  }

  hin_buffer_t * buf = malloc (sizeof (*buf) + sizeof (hin_timeout_t));
  memset (buf, 0, sizeof (*buf));
  buf->flags = 0;
  buf->fd = 0;
  buf->callback = hin_timer_callback;
  buf->count = buf->sz = sizeof (hin_timeout_t);
  buf->ptr = buf->buffer;
  timeout_buffer = buf;

  hin_timeout_t * tm = (void*)&buf->buffer;
  tm->ts.tv_sec = HIN_HTTPD_TIME_DT / 1000;
  tm->ts.tv_nsec = (HIN_HTTPD_TIME_DT % 1000) * 1000000;
  tm->callback = callback;

  if (hin_request_timeout (buf, &tm->ts, 0, 0) < 0) {
    hin_weird_error (212223556);
    return -1;
  }

  return 0;
}



