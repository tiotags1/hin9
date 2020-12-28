
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <hin.h>

static hin_buffer_t * buffer = NULL;

int console_execute (string_t * source) {
  if (match_string (source, "q") > 0) {
    printf ("do quit\n");
    master.quit = 1;
    buffer = NULL;
    //close (0);
  }
}

static int hin_console_read_callback (hin_buffer_t * buf, int ret) {
  if (ret == 0) {
    printf ("free console\n");
    master.quit = 1;
    buffer = NULL;
    return 1;
  }
  string_t temp;
  temp.ptr = buf->ptr;
  temp.len = ret;
  buf->ptr[ret] = '\0';
  console_execute (&temp);
  hin_request_read (buf);
  return 0;
}

void hin_console_clean () {
  if (buffer)
    hin_buffer_clean (buffer);
}

static int hin_timer_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0 && ret != -ETIME) {
    printf ("timer callback error is %s\n", strerror (-ret));
  }
  struct timespec * ts = (struct timespec *)&buffer->buffer;
  hin_request_timeout (buffer, ts, 0, 0);
  void httpd_timer ();
  httpd_timer ();
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

  buf = malloc (sizeof (*buf) + sizeof (struct timespec));
  memset (buf, 0, sizeof (*buf));
  buf->flags = 0;
  buf->fd = 0;
  buf->callback = hin_timer_callback;
  buf->count = buf->sz = sizeof (struct timespec);
  buf->ptr = buf->buffer;
  struct timespec * ts = (struct timespec *)&buf->buffer;
  ts->tv_sec = 1;
  ts->tv_nsec = 0;
  hin_request_timeout (buf, ts, 0, 0);
}




