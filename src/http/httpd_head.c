
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <hin.h>
#include <basic_pattern.h>
#include "http.h"

int header (hin_client_t * client, hin_buffer_t * buffer, const char * fmt, ...) {
  va_list ap;
  va_start (ap, fmt);

  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  int len = vsnprintf (buffer->ptr + pos, sz, fmt, ap);
  //printf ("header send was '%.*s' count was %d\n", len, buffer->ptr + pos, buffer->count);
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

int httpd_parse_headers_line (httpd_client_t * http, string_t * line) {
  string_t param, param1, param2;

  if (match_string (line, "Accept%-Encoding: ") > 0) {
    while (match_string (line, "([%w]+)", &param) > 0) {
      if (match_string (&param, "deflate") > 0) {
        if (master.debug) printf (" can use deflate\n");
        http->flags |= HIN_HTTP_CAN_DEFLATE;
      }
      if (match_string (line, "%s*,%s*") <= 0) break;
    }
  } else if (match_string (line, "Range:%s*") > 0) {
    if (match_string (line, "bytes=(%d+)-(%d*)", &param1, &param2) > 0) {
      http->pos = atoi (param1.ptr);
      if (param2.len > 0) {
        http->count = atoi (param2.ptr);
      } else {
        http->count = -1;
      }
      if (master.debug & DEBUG_PROTO) printf (" range requested is %ld-%ld\n", http->pos, http->count);
    }
  } else if (match_string (line, "If%-Modified%-Since:%s*") > 0) {
    time_t tm = hin_date_str_to_time (line);
    http->modified_since = tm;
  } else if (match_string (line, "If%-None%-Match:%s*\"") > 0) {
    uint64_t etag = strtol (line->ptr, NULL, 16);
    http->etag = etag;
    //printf ("etag is %lx\n", etag);
  } else if (match_string (line, "Connection:%s*") > 0) {
    if (match_string (line, "close") > 0) {
      if (master.debug) printf ("connection requested closed\n");
      http->flags &= ~HIN_HTTP_KEEP;
    } else if (match_string (line, "keep%-alive") > 0) {
      if (master.debug) printf ("connection requested keepalive\n");
      http->flags |= HIN_HTTP_KEEP;
    }
  } else if (http->method == HIN_HTTP_POST) {
    if (match_string (line, "Content%-Length: (%d+)", &param) > 0) {
      http->post_sz = atoi (param.ptr);
      if (master.debug & DEBUG_POST) printf ("Content length is %ld\n", http->post_sz);
    } else if (match_string (line, "Content%-Type: multipart/form%-data; boundary=([%-%w]+)", &param) > 0) {
      http->post_type = HTTP_FORM_MULTIPART;
      char * new = malloc (param.len + 2 + 1);
      new[0] = '-';
      new[1] = '-';
      memcpy (&new[2], param.ptr, param.len);
      new[param.len + 2] = '\0';
      http->post_sep = new;
      if (master.debug & DEBUG_POST) printf ("Content type multipart/form-data boundry is '%s'\n", new);
    }
  }
  return 1;
}

int check_equal_string (string_t * source, const char * format, ...) {
  va_list argptr;
  va_start (argptr, format);
  string_t orig = *source;

  int used = match_string_virtual (source, PATTERN_CASE, format, argptr);

  va_end (argptr);

  *source = orig;
  if (used != orig.len) {
    return -1;
  }

  return used;
}

int httpd_parse_headers (hin_client_t * client, string_t * source) {
  string_t line, method, path, param, param1, param2;
  string_t orig = *source;

  while (1) {
    if (find_line (source, &line) == 0) return 0;
    if (line.len == 0) break;
    if (source->len <= 0) return 0;
  }

  *source = orig;
  httpd_client_t * http = (httpd_client_t*)&client->extra;

  if (find_line (source, &line) == 0 || match_string (&line, "(%a+) ("HIN_HTTP_PATH_ACCEPT") HTTP/1.([01])", &method, &path, &param) <= 0) {
    printf ("httpd 400 error parsing request line\n");
    httpd_respond_error (client, 400, NULL);
    hin_client_shutdown (client);
    return -1;
  }
  if (*param.ptr != '1') {
    http->flags |= HIN_HTTP_VER0;
  } else {
    http->flags |= HIN_HTTP_KEEP;
  }
  if (check_equal_string (&method, "GET") > 0) {
    http->method = HIN_HTTP_GET;
  } else if (check_equal_string (&method, "POST") > 0) {
    http->method = HIN_HTTP_POST;
  } else {
    printf ("httpd 405 error unknown method\n");
    httpd_respond_error (client, 405, NULL);
    hin_client_shutdown (client);
    return -1;
  }

  if (master.debug & DEBUG_HEADERS)
    printf ("method '%.*s' path '%.*s' HTTP/1.%d fd %d\n", (int)method.len, method.ptr,
      (int)path.len, path.ptr, http->flags & HIN_HTTP_VER0 ? 0 : 1, client->sockfd);

  http->count = -1;

  while (find_line (source, &line)) {
    if (master.debug & DEBUG_HEADERS) printf ("header len %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
    if (line.len == 0) break;
    if (httpd_parse_headers_line (http, &line) < 0) {
      *source = orig;
      return -1;
    }
  }

  if (http->method == HIN_HTTP_POST && http->post_sz <= 0) {
    printf ("httpd post missing size\n");
    httpd_respond_error (client, 411, NULL);
    hin_client_shutdown (client);
  }

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}


