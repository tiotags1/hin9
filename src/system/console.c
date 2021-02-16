
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <basic_pattern.h>

#include "hin.h"
#include "utils.h"
#include "conf.h"

static hin_buffer_t * buffer = NULL;
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
  }
  return 0;
}

static int hin_console_read_callback (hin_buffer_t * buf, int ret) {
  if (ret == 0) {
    if (master.debug & DEBUG_OTHER) printf ("console EOF\n");
    hin_stop ();
    return 0;
  }
  string_t temp;
  temp.ptr = buf->ptr;
  temp.len = ret;
  buf->ptr[ret] = '\0';
  hin_request_read (buf);
  console_execute (&temp);
  return 0;
}

void hin_console_clean () {
  if (buffer)
    hin_buffer_clean (buffer);
  if (timeout_buffer)
    hin_buffer_clean (timeout_buffer);
}

static int hin_timer_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0 && ret != -ETIME) {
    printf ("timer callback error is %s\n", strerror (-ret));
  }
  struct timespec * ts = (struct timespec *)&buffer->buffer;
  hin_request_timeout (buffer, ts, 0, 0);
  void httpd_timer ();
  httpd_timer ();
  int hin_timeout_callback (float dt);
  hin_timeout_callback (HIN_HTTPD_TIME_DT / 1000.0f);
  return 0;
}

void hin_console_init () {
  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = 0;
  buf->fd = 0;
  buf->callback = hin_console_read_callback;
  buf->count = buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  hin_request_read (buf);
  buffer = buf;
}

void hin_timer_init () {
  hin_buffer_t * buf = malloc (sizeof (*buf) + sizeof (struct timespec));
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_HIDE;
  buf->fd = 0;
  buf->callback = hin_timer_callback;
  buf->count = buf->sz = sizeof (struct timespec);
  buf->ptr = buf->buffer;
  timeout_buffer = buf;
  struct timespec * ts = (struct timespec *)&buf->buffer;
  ts->tv_sec = HIN_HTTPD_TIME_DT / 1000;
  ts->tv_nsec = (HIN_HTTPD_TIME_DT % 1000) * 1000000;
  hin_request_timeout (buf, ts, 0, 0);
}


