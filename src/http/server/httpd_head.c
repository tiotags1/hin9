
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <basic_pattern.h>

#include "hin.h"
#include "http.h"
#include "conf.h"
#include "vhost.h"

time_t hin_date_str_to_time (string_t * source);

int httpd_vhost_request (httpd_client_t * http, const char * name, int len);

int httpd_parse_headers_line (httpd_client_t * http, string_t * line) {
  string_t param, param1, param2;

  if (matchi_string (line, "Accept-Encoding: ") > 0) {
    while (match_string (line, "([%w]+)", &param) > 0) {
      if (matchi_string (&param, "deflate") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  can use deflate\n");
        http->peer_flags |= HIN_HTTP_DEFLATE;
      } else if (matchi_string (&param, "gzip") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  can use gzip\n");
        http->peer_flags |= HIN_HTTP_GZIP;
      }
      if (match_string (line, "%s*,%s*") <= 0) break;
    }
  } else if (matchi_string (line, "Host:%s*([%w.]+)", &param1) > 0) {
    if (httpd_vhost_request (http, param1.ptr, param1.len) < 0) {
      // can't find hostname
    }
  } else if (matchi_string (line, "If-Modified-Since:%s*") > 0) {
    time_t tm = hin_date_str_to_time (line);
    http->modified_since = tm;
  } else if (matchi_string (line, "If-None-Match:%s*\"") > 0) {
    uint64_t etag = strtol (line->ptr, NULL, 16);
    http->etag = etag;
  } else if (matchi_string (line, "Connection:%s*") > 0) {
    while (1) {
      if (matchi_string (line, "close") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  connection requested closed\n");
        http->peer_flags &= ~HIN_HTTP_KEEPALIVE;
      } else if (matchi_string (line, "keep-alive") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  connection requested keepalive\n");
        http->peer_flags |= HIN_HTTP_KEEPALIVE;
      } else if (match_string (line, "%w+") > 0) {
      }
      if (match_string (line, "%s*,%s*") < 0) break;
    }
  } else if (matchi_string (line, "Range:%s*") > 0) {
    if (matchi_string (line, "bytes=(%d+)-(%d*)", &param1, &param2) > 0) {
      http->pos = atoi (param1.ptr);
      if (param2.len > 0) {
        http->count = atoi (param2.ptr);
      } else {
        http->count = -1;
      }
      if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  range requested is %lld-%lld\n", (long long)http->pos, (long long)http->count);
    }
  } else if (http->method == HIN_METHOD_POST) {
    if (matchi_string (line, "Content-Length: (%d+)", &param) > 0) {
      http->post_sz = atoi (param.ptr);
      if (http->debug & (DEBUG_HTTP|DEBUG_POST)) printf ("  post length is %lld\n", (long long)http->post_sz);
      http->peer_flags |= HIN_HTTP_POST;
    } else if (matchi_string (line, "Content-Type:%s*multipart/form-data;%s*boundary=%\"?([%-%w]+)%\"?", &param) > 0) {
      char * new = malloc (param.len + 2 + 1);
      new[0] = '-';
      new[1] = '-';
      memcpy (&new[2], param.ptr, param.len);
      new[param.len + 2] = '\0';
      http->post_sep = new;
      if (http->debug & (DEBUG_HTTP|DEBUG_POST)) printf ("  post content type multipart/form-data boundry is '%s'\n", new);
    } else if (matchi_string (line, "Transfer-Encoding:%s*") > 0) {
      if (matchi_string (line, "chunked") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_POST)) printf ("  post content encoding is chunked\n");
        http->peer_flags |= (HIN_HTTP_CHUNKED_UPLOAD | HIN_HTTP_POST);
      } else if (matchi_string (line, "identity") > 0) {
      } else {
        httpd_error (http, 0, "doesn't accept post with transfer encoding");
        return -1;
      }
    }
  }
  return 1;
}

int httpd_parse_headers (httpd_client_t * http, string_t * source) {
  string_t line, path;
  string_t orig = *source;

  while (1) {
    if (source->len <= 0) return 0;
    if (hin_find_line (source, &line) == 0) return 0;
    if (line.len == 0) break;
  }

  *source = orig;

  if (HIN_HTTPD_OVERLOAD_ERROR && hin_request_is_overloaded ()) {
    httpd_error (http, 503, "overload");
    return -1;
  }

  line.len = 0;
  int method = 0, version = 0;
  if (hin_find_line (source, &line) == 0
|| hin_http_parse_header_line (&line, &method, &path, &version) < 0
|| version == 0 || method == 0) {
    int status = 400;
    if (version == 0) { status = 505; }
    if (method == 0) { status = 501; }
    httpd_error (http, status, "parsing request line '%.*s'", (int)line.len, line.ptr);
    if (http->debug & (DEBUG_RW|DEBUG_RW_ERROR))
      printf (" raw request '\n%.*s'\n", (int)orig.len, orig.ptr);
    return -1;
  }

  if (version == 0x10) { http->peer_flags |= HIN_HTTP_VER0;
  } else { http->peer_flags |= HIN_HTTP_KEEPALIVE; }

  if (http->debug & (DEBUG_HTTP|DEBUG_RW))
    printf ("httpd %d method %x path '%.*s' ver %x\n", http->c.sockfd, method,
      (int)path.len, path.ptr, version);

  http->count = -1;
  http->method = method;

  while (hin_find_line (source, &line)) {
    if (http->debug & DEBUG_RW) printf (" %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
    if (line.len == 0) break;
    if (httpd_parse_headers_line (http, &line) < 0) {
      *source = orig;
      if (http->status == 200) {
        httpd_error (http, 400, "shouldn't happen");
      }
      return -1;
    }
  }

  if (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) {
    http->post_sz = 0;
  }
  if (http->post_sz < 0) {
    http->post_sz = 0;
  }
  if (HIN_HTTPD_ERROR_MISSING_HOSTNAME && (http->peer_flags & HIN_HTTP_VER0) == 0 && http->hostname == NULL) {
    httpd_error (http, 400, "missing hostname");
    return -1;
  }
  if (http->peer_flags & http->disable & HIN_HTTP_POST) {
    httpd_error (http, 403, "post disabled");
    return -1;
  } if (http->method == HIN_METHOD_POST && (http->peer_flags & HIN_HTTP_POST) == 0) {
    httpd_error (http, 411, "post missing size");
    return -1;
  } else if (HIN_HTTPD_MAX_POST_SIZE && http->post_sz >= HIN_HTTPD_MAX_POST_SIZE) {
    httpd_error (http, 413, "post size %lld >= %ld", (long long)http->post_sz, (long)HIN_HTTPD_MAX_POST_SIZE);
    return -1;
  }

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}

int httpd_parse_req (httpd_client_t * http, string_t * source) {
  int used = httpd_parse_headers (http, source);
  if (used <= 0) return used;

  http->peer_flags &= ~http->disable;
  http->state &= ~HIN_REQ_HEADERS;

  return used;
}


