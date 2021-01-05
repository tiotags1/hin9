
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "lua.h"

void httpd_client_ping (hin_client_t * client, int timeout) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  http->next_time = basic_time_get ();
  http->next_time.sec += timeout;
}

static void httpd_client_timer (hin_client_t * client, basic_time_t * now) {
  httpd_client_t * http = (httpd_client_t *)&client->extra;
  if (http->next_time.sec == 0) return ;
  basic_ftime dt = basic_time_fdiff (now, &http->next_time);
  if (dt < 0) {
    int do_close = 0;
    if (http->state & (HIN_REQ_HEADERS | HIN_REQ_POST)) do_close = 1;
    if (master.debug & DEBUG_TIMER)
      printf ("httpd timer shutdown %d %s%.6f\n", client->sockfd, do_close ? "close " : "", dt);
    if (do_close)
      hin_client_shutdown (client);
  } else {
    if (master.debug & DEBUG_TIMER)
      printf ("httpd timer %d %.6f\n", client->sockfd, dt);
  }
}

void httpd_timer () {
  basic_time_t now = basic_time_get ();
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
    for (hin_client_t * client = bp->active_client; client; client = client->next) {
      httpd_client_timer (client, &now);
    }
  }
}

void httpd_timer_flush () {
  basic_time_t now = basic_time_get ();
  now.sec -= 1;
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
    for (hin_client_t * client = bp->active_client; client; client = client->next) {
      httpd_client_t * http = (httpd_client_t *)&client->extra;
      http->next_time = now;
    }
  }
}

void httpd_close_socket () {
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
    close (server->sockfd);
  }
}

int header (hin_client_t * client, hin_buffer_t * buffer, const char * fmt, ...) {
  va_list ap;
  va_start (ap, fmt);

  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  int len = vsnprintf (buffer->ptr + pos, sz, fmt, ap);
  //printf ("header send was '%.*s' count was %d\n", len, buffer->ptr + pos, buffer->count);
  if (len >= sz) { va_end (ap); return 0; }
  buffer->count += len;

  va_end(ap);
  return len;
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

