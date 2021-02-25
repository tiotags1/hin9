
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

int http_parse_headers_line (http_client_t * http, string_t * line) {
  string_t param, param1, param2;
  if (matchi_string_equal (line, "Content-Length: (%d+)", &param) > 0) {
    http->sz = atoi (param.ptr);
    if (http->debug & DEBUG_HTTP_FILTER) printf ("  content length is %ld\n", http->sz);
  } else if (matchi_string (line, "Transfer-Encoding:%s*") > 0) {
    if (matchi_string (line, "chunked") > 0) {
      if (http->debug & DEBUG_HTTP_FILTER) printf ("  transfer encoding is chunked\n");
      http->flags |= HIN_HTTP_CHUNKED;
    } else if (matchi_string (line, "identity") > 0) {
    } else {
      printf ("http %d transport encoding '%.*s' not supported\n", http->c.sockfd, (int)line->len, line->ptr);
      return -1;
    }
  }
  return 1;
}

int http_parse_headers (hin_client_t * client, string_t * source) {
  string_t line, method, path, param, param1, param2;
  string_t orig = *source;

  while (1) {
    if (find_line (source, &line) == 0) return 0;
    if (line.len == 0) break;
    if (source->len <= 0) return 0;
  }

  *source = orig;
  http_client_t * http = (http_client_t*)client;

  if (find_line (source, &line) == 0 || match_string (&line, "HTTP/1.%d ([%w%/]+) %w+", &param1) <= 0) {
    printf ("http %d parsing error\n", http->c.sockfd);
    // close connection return error
    return -1;
  }

  if (http->debug & DEBUG_RW) printf ("http %d headers\n", http->c.sockfd);
  while (find_line (source, &line)) {
    if (http->debug & DEBUG_RW) printf (" %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
    if (line.len == 0) break;
    if (http_parse_headers_line (http, &line) < 0) {
      *source = orig;
      return -1;
    }
  }

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}

