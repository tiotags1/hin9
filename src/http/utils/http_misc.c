
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
    if (http->debug & DEBUG_TIMEOUT)
      printf ("httpd %d timer shutdown state %x %s %.6f\n", http->c.sockfd, http->state, do_close ? "close" : "wait", dt);
    if (do_close) {
      shutdown (http->c.sockfd, SHUT_RD);
      httpd_client_shutdown (http);
    }
  } else {
    if (http->debug & DEBUG_TIMEOUT)
      printf ("httpd %d timer %.6f\n", http->c.sockfd, dt);
  }
}

void httpd_timer () {
  basic_time_t now = basic_time_get ();
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_t * bp = (hin_server_t*)server;
    for (httpd_client_t * http = (httpd_client_t*)bp->active_client; http; http = (httpd_client_t*)http->c.next) {
      httpd_client_timer (http, &now);
    }
  }
}

void httpd_timer_flush () {
  basic_time_t now = basic_time_get ();
  now.sec -= 1;
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    hin_server_t * bp = (hin_server_t*)server;
    for (hin_client_t * client = bp->active_client; client; client = client->next) {
      httpd_client_t * http = (httpd_client_t *)client;
      http->next_time = now;
    }
  }
}

void httpd_close_socket () {
  for (hin_client_t * server = master.server_list; server; server = server->next) {
    close (server->sockfd);
  }
}

int httpd_parse_cache_str (const char * str, size_t len, uint32_t * flags_out, time_t * max_age) {
  string_t source, opt, param1;
  source.ptr = (char *)str;
  source.len = len;
  if (len == 0) source.len = strlen (str);
  uint32_t flags = 0;
  while (match_string (&source, "%s*([%w%-=]+)", &opt) > 0) {
    if (match_string (&opt, "max-age=%s*(%d+)", &param1) > 0) {
      if (max_age) *max_age = atoi (param1.ptr);
      flags |= HIN_CACHE_MAX_AGE;
    } else if (match_string (&opt, "public") > 0) {
      flags |= HIN_CACHE_PUBLIC;
    } else if (match_string (&opt, "private") > 0) {
      flags |= HIN_CACHE_PRIVATE;
    } else if (match_string (&opt, "immutable") > 0) {
      flags |= HIN_CACHE_IMMUTABLE;
    } else if (match_string (&opt, "no-cache") > 0) {
      flags |= HIN_CACHE_NO_CACHE;
    } else if (match_string (&opt, "no-store") > 0) {
      flags |= HIN_CACHE_NO_STORE;
    } else if (match_string (&opt, "no-transform") > 0) {
      flags |= HIN_CACHE_NO_TRANSFORM;
    } else if (match_string (&opt, "must-revalidate") > 0) {
      flags |= HIN_CACHE_MUST_REVALIDATE;
    } else if (match_string (&opt, "proxy-revalidate") > 0) {
      flags |= HIN_CACHE_PROXY_REVALIDATE;
    } else if (match_string (&opt, "no-store") > 0) {
      flags |= HIN_CACHE_NO_STORE;
    }
    if (match_string (&source, "%s*,%s*") <= 0) break;
  }

  if (flags_out) *flags_out = flags;
  return 0;
}

int header_cache_control (hin_buffer_t * buf, uint32_t flags, time_t max_age) {
  int num = 0;
  num += header (buf, "Cache-Control: ");
  if (flags & HIN_CACHE_PRIVATE) num += header (buf, "private, ");
  else if (flags & HIN_CACHE_PUBLIC) num += header (buf, "public, ");
  if (flags & HIN_CACHE_MAX_AGE) num += header (buf, "max-age=%ld, ", max_age);
  if (flags & HIN_CACHE_IMMUTABLE) num += header (buf, "immutable, ");
  else if (flags & HIN_CACHE_NO_CACHE) num += header (buf, "no-cache, ");
  if (flags & HIN_CACHE_NO_STORE) num += header (buf, "no-store, ");
  if (flags & HIN_CACHE_NO_TRANSFORM) num += header (buf, "no-transform, ");
  if (flags & HIN_CACHE_MUST_REVALIDATE) num += header (buf, "must-revalidate, ");
  if (flags & HIN_CACHE_PROXY_REVALIDATE) num += header (buf, "proxy-revalidate, ");

  hin_buffer_t * last = buf;
  while (last->next) { last = last->next; }
  last->count -= 2;
  header (buf, "\r\n");
  return num;
}

void hin_cache_set_number (httpd_client_t * http, time_t num) {
  http->cache = num;
  if (num > 0) {
    http->cache_flags = HIN_CACHE_PUBLIC | HIN_CACHE_IMMUTABLE | HIN_CACHE_MAX_AGE;
  } else if (num < 0) {
    http->cache_flags = HIN_CACHE_NO_STORE | HIN_CACHE_NO_CACHE;
  } else {
    http->cache_flags = 0;
  }
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


