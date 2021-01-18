
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

int http_parse_headers_line (http_client_t * http, string_t * line) {
  string_t param, param1, param2;
  if (match_string (line, "Content%-Length: (%d+)", &param) > 0) {
    http->sz = atoi (param.ptr);
    printf ("Content length is %ld\n", http->sz);
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
    printf ("httpd parsing error\n");
    // close connection return error
    return -1;
  }

  while (find_line (source, &line)) {
    if (master.debug & DEBUG_HEADERS) printf ("header len %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
    if (line.len == 0) break;
    if (http_parse_headers_line (http, &line) < 0) {
      *source = orig;
      return -1;
    }
  }

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}

