
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>

#include <basic_pattern.h>

#include "hin.h"
#include "utils.h"
#include "conf.h"
#include "system/hin_lua.h"

static hin_buffer_t * console_buffer = NULL;

void hin_stop1 ();

int console_execute (string_t * source) {
  if (matchi_string_equal (source, "q\n") > 0) {
    printf ("do quit\n");
    hin_stop1 ();
  } else if (matchi_string_equal (source, "restart\n") > 0) {
    hin_restart1 ();
  } else if (matchi_string_equal (source, "reload\n") > 0) {
    int lua_reload ();
    if (lua_reload () < 0)
      printf ("reload failed\n");
  } else if (matchi_string (source, "lua ") > 0) {
    hin_lua_run (source->ptr, source->len);
  } else {
    hin_lua_run (source->ptr, source->len);
  }
  return 0;
}

static int hin_console_read_callback (hin_buffer_t * buf, int ret) {
  if (ret <= 0) {
    if (ret < 0) {
      printf ("console error %s\n", strerror (-ret));
    }
    if (master.debug & DEBUG_CONFIG) printf ("console EOF\n");
    hin_stop1 ();
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
  if (console_buffer) {
    hin_buffer_stop_clean (console_buffer);
  }
}

int hin_timer_cb (int ms) {
  int hin_check_alive_timer ();
  hin_check_alive_timer ();
  int hin_timeout_callback (float dt);
  hin_timeout_callback (1);
  int hin_timer_check ();
  hin_timer_check ();
  return 0;
}

int hin_console_init () {
  hin_timer_init (hin_timer_cb);

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



