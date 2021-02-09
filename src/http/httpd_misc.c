
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>

#include "hin.h"
#include "http.h"
#include "lua.h"

void httpd_client_ping (httpd_client_t * http, int timeout) {
  http->next_time = basic_time_get ();
  http->next_time.sec += timeout;
}

static void httpd_client_timer (httpd_client_t * http, basic_time_t * now) {
  if (http->next_time.sec == 0) return ;
  basic_ftime dt = basic_time_fdiff (now, &http->next_time);
  if (dt < 0.0) {
    int do_close = 0;
    if (http->state & (HIN_REQ_HEADERS | HIN_REQ_POST | HIN_REQ_END)) do_close = 1;
    if (master.debug & DEBUG_TIMER)
      printf ("httpd timer shutdown %d state %x %s%.6f\n", http->c.sockfd, http->state, do_close ? "close " : "", dt);
    if (do_close) {
      shutdown (http->c.sockfd, SHUT_RD);
      httpd_client_shutdown (http);
    }
  } else {
    if (master.debug & DEBUG_TIMER)
      printf ("httpd timer %d %.6f\n", http->c.sockfd, dt);
  }
}

void httpd_timer () {
  basic_time_t now = basic_time_get ();
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;
    for (httpd_client_t * http = (httpd_client_t*)bp->active_client; http; http = (httpd_client_t*)http->c.next) {
      httpd_client_timer (http, &now);
    }
  }
}

void httpd_timer_flush () {
  basic_time_t now = basic_time_get ();
  now.sec -= 1;
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;
    for (hin_client_t * client = bp->active_client; client; client = client->next) {
      httpd_client_t * http = (httpd_client_t *)client;
      http->next_time = now;
    }
  }
}

void httpd_close_socket () {
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;
    close (server->sockfd);
  }
}

static hin_buffer_t * new_buffer (hin_buffer_t * buffer) {
  printf ("header needed to make new buffer\n");
  hin_buffer_t * buf = calloc (1, sizeof (hin_buffer_t) + READ_SZ);
  buf->sz = READ_SZ;
  buf->fd = buffer->fd;
  buf->flags = buffer->flags;
  buf->callback = buffer->callback;
  buf->parent = buffer->parent;
  buf->ptr = buf->buffer;
  buf->ssl = buffer->ssl;
  buf->count = 0;
  buffer->next = buf;
  buf->prev = buffer;
  return buf;
}

static int vheader (hin_client_t * client, hin_buffer_t * buffer, const char * fmt, va_list ap) {
  if (buffer->next) return vheader (client, buffer->next, fmt, ap);
  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  va_list prev;
  va_copy (prev, ap);
  int len = vsnprintf (buffer->ptr + pos, sz, fmt, ap);
  if (len > sz) {
    if (len > READ_SZ) {
      printf ("'header' failed to write more\n");
      va_end (ap);
      return 0;
    }
    hin_buffer_t * buf = new_buffer (buffer);
    return vheader (client, buf, fmt, prev);
  }
  buffer->count += len;
  return len;
}

int header (hin_client_t * client, hin_buffer_t * buffer, const char * fmt, ...) {
  va_list ap;
  va_start (ap, fmt);

  int len = vheader (client, buffer, fmt, ap);

  va_end (ap);
  return len;
}

int header_raw (hin_client_t * client, hin_buffer_t * buffer, const char * data, int len) {
  if (buffer->next) return header_raw (client, buffer->next, data, len);

  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  if (len > sz) return 0;
  if (len > sz) {
    if (len > READ_SZ) {
      printf ("'header_raw' failed to write more\n");
      return 0;
    }
    hin_buffer_t * buf = new_buffer (buffer);
    return header_raw (client, buf, data, len);
  }

  memcpy (buffer->ptr + pos, data, len);
  buffer->count += len;
  return len;
}

int header_date (hin_client_t * client, hin_buffer_t * buf, const char * name, time_t time) {
  char buffer[80];
  struct tm *info = gmtime (&time);
  strftime (buffer, sizeof buffer, "%a, %d %b %Y %X GMT", info);
  return header (client, buf, "%s: %s\r\n", name, buffer);
}

time_t hin_date_str_to_time (string_t * source) {
  struct tm tm;
  time_t t;
  if (strptime (source->ptr, "%a, %d %b %Y %X GMT", &tm) == NULL) {
    printf ("can't strptime\n");
    return 0;
  }
  tm.tm_isdst = -1; // Not set by strptime(); tells mktime() to determine whether daylight saving time is in effect
  t = mktime (&tm);
  if (t == -1) {
    printf ("can't mktime\n");
    return 0;
  }
  return t;
}


