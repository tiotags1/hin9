
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <basic_pattern.h>

#include "hin.h"
#include "utils.h"
#include "conf.h"

static hin_buffer_t * console_buffer = NULL;
static hin_buffer_t * timeout_buffer = NULL;

void hin_stop ();

int console_execute (string_t * source) {
  if (matchi_string_equal (source, "q\n") > 0) {
    printf ("do quit\n");
    hin_stop ();
  } else if (matchi_string_equal (source, "restart\n") > 0) {
    int hin_restart ();
    hin_restart ();
  } else if (matchi_string_equal (source, "reload\n") > 0) {
    int lua_reload ();
    if (lua_reload () < 0)
      printf ("reload failed\n");
  } else {
    printf ("command unknown '%.*s'\n", (int)source->len, source->ptr);
  }
  return 0;
}

static int hin_console_read_callback (hin_buffer_t * buf, int ret) {
  if (ret <= 0) {
    if (ret < 0) {
      printf ("console error %s\n", strerror (-ret));
    }
    if (master.debug & DEBUG_CONFIG) printf ("console EOF\n");
    hin_stop ();
    return 0;
  }
  string_t temp;
  temp.ptr = buf->ptr;
  temp.len = ret;
  buf->ptr[ret] = '\0';
  console_execute (&temp);
  if (hin_request_read (buf) < 0) {
    printf ("console failed to read\n");
    return -1;
  }
  return 0;
}

void hin_console_clean () {
  if (console_buffer)
    hin_buffer_clean (console_buffer);
  if (timeout_buffer)
    hin_buffer_clean (timeout_buffer);
}

int hin_console_init () {
  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  #ifdef HIN_LINUX_BUG_5_11_3
  buf->flags = HIN_EPOLL;
  #endif
  buf->fd = STDIN_FILENO;
  buf->callback = hin_console_read_callback;
  buf->count = buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->debug = master.debug;
  if (hin_request_read (buf) < 0) {
    printf ("console init failed\n");
    hin_buffer_clean (buf);
    return -1;
  }
  console_buffer = buf;
  return 0;
}

#include <basic_timer.h>

typedef struct {
  basic_timer_t timer;
  struct timespec ts;
  int pad;
} hin_timeout_t;

static int hin_timer_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0 && ret != -ETIME) {
    printf ("timer callback error is %s\n", strerror (-ret));
  }
  hin_timeout_t * tm = (void*)&buffer->buffer;
  if (hin_request_timeout (buffer, &tm->ts, 0, 0) < 0) {
    printf ("timeout callback failed\n");
    return -1;
  }

  void httpd_timer ();
  httpd_timer ();
  int hin_check_alive_timer ();
  hin_check_alive_timer ();

  int hin_epoll_check ();
  hin_epoll_check ();

  int frames = basic_timer_frames (&tm->timer);

  int hin_timeout_callback (float dt);
  hin_timeout_callback (tm->timer.dt);
  void hin_cache_timer (int num);
  if (frames > 0)
    hin_cache_timer (frames);

  return 0;
}

int hin_timer_init () {
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
  if (hin_request_timeout (buf, &tm->ts, 0, 0) < 0) {
    printf ("timeout callback failed to init\n");
    return -1;
  }

  basic_timer_init (&tm->timer, 1);
  return 0;
}


