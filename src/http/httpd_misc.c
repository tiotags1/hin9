
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>

#include "hin.h"
#include "http.h"
#include "lua.h"
#include "file.h"

void httpd_client_ping (httpd_client_t * http, int timeout) {
  http->next_time = basic_time_get ();
  http->next_time.sec += timeout;
}

static inline void httpd_client_timer (httpd_client_t * http, basic_time_t * now) {
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

int httpd_parse_cache_str (string_t * orig, hin_cache_data_t * cache) {
  string_t source = *orig, opt, param1;
  while (match_string (&source, "%s*([%w%-=]+)", &opt) > 0) {
    if (match_string (&opt, "max-age=%s*(%d+)", &param1) > 0) {
      cache->max_age = atoi (param1.ptr);
    }
    if (match_string (&source, "%s*,%s*") <= 0) break;
  }
  return 0;
}

time_t hin_date_str_to_time (string_t * source) {
  struct tm tm;
  time_t t;
  if (strptime (source->ptr, "%a, %d %b %Y %X GMT", &tm) == NULL) {
    printf ("can't strptime '%.*s'\n", 20, source->ptr);
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


