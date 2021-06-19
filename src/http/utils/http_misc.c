
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <sys/socket.h>

#include "hin.h"
#include "http.h"
#include "file.h"

int httpd_timeout_callback (hin_timer_t * timer, time_t tm) {
  httpd_client_t * http = (httpd_client_t*)timer->ptr;
  int do_close = 0;
  if (http->state & (HIN_REQ_HEADERS | HIN_REQ_POST | HIN_REQ_END)) do_close = 1;
  if (http->debug & DEBUG_TIMEOUT)
    printf ("httpd %d timer shutdown %lld state %x %s\n", http->c.sockfd, (long long)tm, http->state, do_close ? "close" : "wait");
  if (do_close == 0) {
    hin_timer_update (timer, time (NULL) + 5);
    return 0;
  }
  shutdown (http->c.sockfd, SHUT_RD);
  httpd_client_shutdown (http);
  return 0;
}

void httpd_client_ping (httpd_client_t * http, int timeout) {
  hin_timer_t * timer = &http->timer;
  time_t tm = time (NULL) + timeout;
  hin_timer_update (timer, tm);
  if (http->debug & DEBUG_TIMEOUT)
    printf ("httpd %d timeout %p at %lld\n", http->c.sockfd, timer->ptr, (long long)timer->time);
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


