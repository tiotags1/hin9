
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

static int http_parse_headers_line (http_client_t * http, string_t * line) {
  string_t param;
  if (matchi_string_equal (line, "Content-Length: (%d+)", &param) > 0) {
    http->sz = atoi (param.ptr);
    if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER))
      printf ("  content length is %lld\n", (long long)http->sz);
  } else if (matchi_string (line, "Transfer-Encoding:%s*") > 0) {
    if (matchi_string (line, "chunked") > 0) {
      if (http->debug & (DEBUG_HTTP|DEBUG_HTTP_FILTER))
        printf ("  transfer encoding is chunked\n");
      http->flags |= HIN_HTTP_CHUNKED;
    } else if (matchi_string (line, "identity") > 0) {
    } else {
      printf ("http %d transport encoding '%.*s' not supported\n", http->c.sockfd, (int)line->len, line->ptr);
      return -1;
    }
  }
  return 1;
}

static int http_parse_headers (http_client_t * http, string_t * source) {
  string_t line, param1;
  string_t orig = *source;

  while (1) {
    if (find_line (source, &line) == 0) return 0;
    if (line.len == 0) break;
    if (source->len <= 0) return 0;
  }

  http->io_state &= ~HIN_REQ_HEADERS;

  *source = orig;

  if (find_line (source, &line) == 0 || match_string (&line, "HTTP/1.%d ([%d]+) %w+", &param1) <= 0) {
    httpc_error (http, 0, "http parsing error");
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

  hin_pipe_t * http_client_start_pipe (http_client_t * http, string_t * source);
  hin_pipe_t * pipe = http_client_start_pipe (http, source);

  int used = hin_http_state (http, HIN_HTTP_STATE_HEADERS, (uintptr_t)pipe);
  if (used > 0) {
    return used;
  }

  hin_pipe_start (pipe);

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}

static int http_client_headers_close_callback (hin_buffer_t * buf, int ret) {
  http_client_t * http = buf->parent;
  if ((http->io_state & HIN_REQ_HEADERS)) {
    hin_http_state (http, HIN_HTTP_STATE_HEADERS_FAILED, ret);
  }
  http_client_shutdown (http);
  return 0;
}

static int http_client_headers_read_callback (hin_buffer_t * buffer, int received) {
  http_client_t * http = (http_client_t*)buffer->parent;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  string_t data;
  data.ptr = lines->base;
  data.len = lines->count;

  int used = http_parse_headers (http, &data);

  return used;
}

static int http_client_sent_callback (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    http_client_t * http = buf->parent;
    printf ("http %d header send failed %s\n", buf->fd, strerror (-ret));
    hin_http_state (http, HIN_HTTP_STATE_HEADERS_FAILED, ret);
    http_client_shutdown (http);
  }
  return 1;
}

int http_client_start_headers (http_client_t * http, hin_buffer_t * received) {
  if (http->debug & DEBUG_HTTP) printf ("http %d request begin\n", http->c.sockfd);

  hin_lines_t * lines = (hin_lines_t*)&http->read_buffer->buffer;
  lines->read_callback = http_client_headers_read_callback;
  lines->close_callback = http_client_headers_close_callback;

  http->io_state |= HIN_REQ_HEADERS;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = http_client_sent_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  char * path = http->uri.path.ptr;
  char * path_max = path + http->uri.path.len;
  if (http->uri.query.len > 0) {
    path_max = http->uri.query.ptr + http->uri.query.len;
  }

  header (buf, "GET %.*s HTTP/1.1\r\n", path_max - path, path);
  if (http->uri.port.len > 0) {
    header (buf, "Host: %.*s:%.*s\r\n", http->uri.host.len, http->uri.host.ptr, http->uri.port.len, http->uri.port.ptr);
  } else {
    header (buf, "Host: %.*s\r\n", http->uri.host.len, http->uri.host.ptr);
  }
  if (http->flags & HIN_HTTP_KEEPALIVE) {
    header (buf, "Connection: keep-alive\r\n");
  } else {
    header (buf, "Connection: close\r\n");
  }
  header (buf, "\r\n");

  if (http->debug & DEBUG_RW) printf ("http %d request '\n%.*s'\n", http->c.sockfd, buf->count, buf->ptr);

  if (hin_request_write (buf) < 0) {
    http_client_shutdown (http);
    return -1;
  }
  return 0;
}

