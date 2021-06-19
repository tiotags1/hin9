
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
#include "hin_lua.h"

time_t hin_date_str_to_time (string_t * source);

int httpd_vhost_request (httpd_client_t * http, const char * name, int len);

int httpd_parse_headers_line (httpd_client_t * http, string_t * line) {
  string_t param, param1, param2;

  if (match_string (line, "Accept%-Encoding: ") > 0) {
    while (match_string (line, "([%w]+)", &param) > 0) {
      if (match_string (&param, "deflate") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  can use deflate\n");
        http->peer_flags |= HIN_HTTP_DEFLATE;
      }
      if (match_string (line, "%s*,%s*") <= 0) break;
    }
  } else if (match_string (line, "Host:%s*([%w%.]+)", &param1) > 0) {
    if (httpd_vhost_request (http, param1.ptr, param1.len) < 0) {
      // can't find hostname
    }
  } else if (match_string (line, "If%-Modified%-Since:%s*") > 0) {
    time_t tm = hin_date_str_to_time (line);
    http->modified_since = tm;
  } else if (match_string (line, "If%-None%-Match:%s*\"") > 0) {
    uint64_t etag = strtol (line->ptr, NULL, 16);
    http->etag = etag;
  } else if (match_string (line, "Connection:%s*") > 0) {
    if (match_string (line, "close") > 0) {
      if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  connection requested closed\n");
      http->peer_flags &= ~HIN_HTTP_KEEPALIVE;
    } else if (match_string (line, "keep%-alive") > 0) {
      if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  connection requested keepalive\n");
      http->peer_flags |= HIN_HTTP_KEEPALIVE;
    }
  } else if (match_string (line, "Range:%s*") > 0) {
    if (match_string (line, "bytes=(%d+)-(%d*)", &param1, &param2) > 0) {
      http->pos = atoi (param1.ptr);
      if (param2.len > 0) {
        http->count = atoi (param2.ptr);
      } else {
        http->count = -1;
      }
      if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER)) printf ("  range requested is %lld-%lld\n", (long long)http->pos, (long long)http->count);
    }
  } else if (http->method == HIN_HTTP_POST) {
    if (match_string (line, "Content-Length: (%d+)", &param) > 0) {
      http->post_sz = atoi (param.ptr);
      if (http->debug & (DEBUG_HTTP|DEBUG_POST)) printf ("  post length is %lld\n", (long long)http->post_sz);
    } else if (match_string (line, "Content-Type:%s*multipart/form-data;%s*boundary=%\"?([%-%w]+)%\"?", &param) > 0) {
      char * new = malloc (param.len + 2 + 1);
      new[0] = '-';
      new[1] = '-';
      memcpy (&new[2], param.ptr, param.len);
      new[param.len + 2] = '\0';
      http->post_sep = new;
      if (http->debug & (DEBUG_HTTP|DEBUG_POST)) printf ("  post content type multipart/form-data boundry is '%s'\n", new);
    } else if (match_string (line, "Transfer-Encoding:%s*") > 0) {
      if (match_string (line, "chunked") > 0) {
        if (http->debug & (DEBUG_HTTP|DEBUG_POST)) printf ("  post content encoding is chunked\n");
        http->peer_flags |= HIN_HTTP_CHUNKED_UPLOAD;
      } else if (match_string (line, "identity") > 0) {
      } else {
        printf ("httpd %d don't accept post with transfer encoding\n", http->c.sockfd);
        return -1;
      }
    }
  }
  return 1;
}

int httpd_parse_headers (httpd_client_t * http, string_t * source) {
  string_t line, method, path, param;
  string_t orig = *source;

  while (1) {
    if (source->len <= 0) return 0;
    if (find_line (source, &line) == 0) return 0;
    if (line.len == 0) break;
  }

  *source = orig;

  if (HIN_HTTPD_OVERLOAD_ERROR && hin_request_is_overloaded ()) {
    httpd_respond_fatal (http, 503, NULL);
    return -1;
  }

  line.len = 0;
  if (find_line (source, &line) == 0 || match_string (&line, "(%a+) ("HIN_HTTP_PATH_ACCEPT") HTTP/1.([01])", &method, &path, &param) <= 0) {
    printf ("httpd 400 error parsing request line '%.*s'\n", (int)line.len, line.ptr);
    if (http->debug & (DEBUG_RW|DEBUG_RW_ERROR))
      printf (" raw request '\n%.*s'\n", (int)orig.len, orig.ptr);
    httpd_respond_fatal (http, 400, NULL);
    return -1;
  }
  if (*param.ptr != '1') {
    http->peer_flags |= HIN_HTTP_VER0;
  } else {
    http->peer_flags |= HIN_HTTP_KEEPALIVE;
  }
  if (matchi_string_equal (&method, "GET") > 0) {
    http->method = HIN_HTTP_GET;
  } else if (matchi_string_equal (&method, "POST") > 0) {
    http->method = HIN_HTTP_POST;
  } else if (matchi_string_equal (&method, "HEAD") > 0) {
    http->method = HIN_HTTP_HEAD;
  } else {
    printf ("httpd 405 error unknown method '%.*s'\n", (int)method.len, method.ptr);
    if (http->debug & (DEBUG_RW|DEBUG_RW_ERROR))
      printf (" raw request '\n%.*s'\n", (int)orig.len, orig.ptr);
    httpd_respond_fatal (http, 405, NULL);
    return -1;
  }

  if (http->debug & (DEBUG_HTTP|DEBUG_RW))
    printf ("httpd %d method '%.*s' path '%.*s' HTTP/1.%d\n", http->c.sockfd, (int)method.len, method.ptr,
      (int)path.len, path.ptr, http->peer_flags & HIN_HTTP_VER0 ? 0 : 1);

  http->count = -1;

  while (find_line (source, &line)) {
    if (http->debug & DEBUG_RW) printf (" %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
    if (line.len == 0) break;
    if (httpd_parse_headers_line (http, &line) < 0) {
      *source = orig;
      if (http->status == 200) {
        httpd_respond_fatal (http, 400, NULL);
      }
      return -1;
    }
  }

  if (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) {
    http->post_sz = 0;
  }
  if (http->method == HIN_HTTP_POST && (http->post_sz <= 0 && (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) == 0)) {
    printf ("httpd post missing size\n");
    httpd_respond_fatal (http, 411, NULL);
    return -1;
  } else if (HIN_HTTPD_MAX_POST_SIZE && http->post_sz >= HIN_HTTPD_MAX_POST_SIZE) {
    printf ("httpd post size %lld >= %ld\n", (long long)http->post_sz, (long)HIN_HTTPD_MAX_POST_SIZE);
    httpd_respond_fatal (http, 413, NULL);
    return -1;
  }

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}

int httpd_parse_req (httpd_client_t * http, string_t * source) {
  int used = httpd_parse_headers (http, source);
  if (used <= 0) return used;

  if (http->disable & HIN_HTTP_KEEPALIVE) {
    http->peer_flags &= ~HIN_HTTP_KEEPALIVE;
  }
  http->state &= ~HIN_REQ_HEADERS;

  return used;
}


