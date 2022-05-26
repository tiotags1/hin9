
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
  } else if (matchi_string (line, "Cache-Control:") > 0) {
    httpd_parse_cache_str (line->ptr, line->len, &http->cache_flags, &http->cache);
  }
  return 1;
}

static int http_client_headers_read_callback (hin_buffer_t * buffer, int received) {
  http_client_t * http = (http_client_t*)buffer->parent;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  string_t data, *source;
  data.ptr = lines->base;
  data.len = lines->count;
  source = &data;

  if (http->io_state & HIN_REQ_IDLE) return 0;

  string_t line, param1;
  string_t orig = *source;

  while (1) {
    if (hin_find_line (source, &line) == 0) return 0;
    if (line.len == 0) break;
    if (source->len <= 0) return 0;
  }

  *source = orig;

  if (hin_find_line (source, &line) == 0 || match_string (&line, "HTTP/1.%d ([%d]+) %w+", &param1) <= 0) {
    httpc_error (http, 0, "http parsing error");
    // close connection return error
    return -1;
  }

  http->status = atoi (param1.ptr);

  if (http->debug & DEBUG_RW) printf ("http %d headers status %d\n", http->c.sockfd, http->status);
  while (hin_find_line (source, &line)) {
    if (http->debug & DEBUG_RW) printf (" %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
    if (line.len == 0) break;
    if (http_parse_headers_line (http, &line) < 0) {
      *source = orig;
      return -1;
    }
  }

  hin_pipe_t * http_client_start_pipe (http_client_t * http, string_t * source);
  hin_pipe_t * pipe = http_client_start_pipe (http, source);

  hin_pipe_start (pipe);

  return (uintptr_t)source->ptr - (uintptr_t)orig.ptr;
}

static int http_client_restart (http_client_t * old, httpd_client_t * parent) {
  if (parent == NULL) return 0;

  http_client_t * http = http_connection_get (old->uri.all.ptr);

  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    http->flags |= HIN_HTTP_KEEPALIVE;
  }

  http->c.parent = parent;
  http->debug = parent->debug;

  http->read_callback = old->read_callback;
  http->state_callback = old->state_callback;

  http_connection_start (http);

  old->c.parent = NULL;
  old->read_callback = NULL;
  old->state_callback = NULL;

  return 0;
}

static int http_client_headers_close_callback (hin_buffer_t * buf, int ret) {
  http_client_t * http = buf->parent;
  httpd_client_t * parent = http->c.parent;

  if (ret != 0)
    printf ("headers closed %d bc %d %s\n", buf->fd, ret, strerror (-ret));

  if (http->io_state & (HIN_REQ_HEADERS|HIN_REQ_DATA)) {
    //if (ret != 0)
    //  hin_http_state (http, HIN_HTTP_STATE_HEADERS_FAILED, ret);

    if ((http->io_state & HIN_REQ_IDLE) == 0)
      http_client_restart (http, parent);
  }

  http_client_shutdown (http);

  return 0;
}

static int http_client_sent_callback (hin_buffer_t * buf, int ret) {
  http_client_t * http = buf->parent;
  http->io_state &= ~HIN_REQ_HEADERS;

  if (http->io_state & HIN_REQ_STOPPING) {
    http_client_shutdown (http);
    return 1;
  }

  if (ret <= 0) {
    printf ("http %d send error %s\n", buf->fd, strerror (-ret));
    hin_http_state (http, HIN_HTTP_STATE_HEADERS_FAILED, ret);
    http_client_shutdown (http);
  }

  return 1;
}

int http_client_start_headers (http_client_t * http, hin_buffer_t * received) {
  if (http->debug & DEBUG_HTTP) printf ("http %d '%s' begin\n", http->c.sockfd, http->uri.all.ptr);

  hin_lines_t * lines = (hin_lines_t*)&http->read_buffer->buffer;
  lines->read_callback = http_client_headers_read_callback;
  lines->close_callback = http_client_headers_close_callback;

  http->io_state |= HIN_REQ_HEADERS|HIN_REQ_DATA;;

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

  const char * method = "GET";
  switch (http->method) {
  case HIN_METHOD_POST: method = "POST"; break;
  }

  header (buf, "%s %.*s HTTP/1.1\r\n", method, path_max - path, path);
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

  if (hin_http_state (http, HIN_HTTP_STATE_SEND, (uintptr_t)buf) == 0)
    header (buf, "\r\n");

  if (http->debug & DEBUG_RW) printf ("http %d request '\n%.*s'\n", http->c.sockfd, buf->count, buf->ptr);

  if (hin_request_write (buf) < 0) {
    http_client_shutdown (http);
    return -1;
  }
  return 0;
}

