
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

static int httpd_pipe_error_callback (hin_pipe_t * pipe, int err) {
  printf ("http %d error!\n", pipe->out.fd);
  http_client_t * http = pipe->parent;
  httpd_client_shutdown (http->c.parent);
  return 0;
}

static int hin_rproxy_headers (http_client_t * http, hin_pipe_t * pipe) {
  httpd_client_t * parent = http->c.parent;
  pipe->out.flags &= ~(HIN_FILE | HIN_OFFSETS);

  parent->count = http->sz;

  httpd_pipe_set_http11_response_options (parent, pipe);
  pipe->out_error_callback = httpd_pipe_error_callback;
  pipe->parent = http;
  pipe->parent1 = parent;

  if ((http->flags & HIN_HTTP_CHUNKED)) {
    pipe->left = pipe->sz = 0;
    pipe->in.flags &= ~HIN_COUNT;
  } else {
    pipe->left = pipe->sz = http->sz;
    pipe->in.flags |= HIN_COUNT;
  }

  hin_buffer_t * header_buf = http->read_buffer;
  hin_lines_t * lines = (hin_lines_t*)&header_buf->buffer;
  string_t source, line, param;
  source.ptr = lines->base;
  source.len = lines->count;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->fd = parent->c.sockfd;
  buf->flags = parent->c.flags;
  buf->ssl = &parent->c.ssl;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = pipe;
  buf->debug = parent->debug;

  find_line (&source, &line);
  if (matchi_string (&line, "HTTP/1.%d (%d+)", &param) <= 0) {
  }
  parent->status = atoi (param.ptr);

  header (buf, "HTTP/1.%d %d %s\r\n", parent->peer_flags & HIN_HTTP_VER0 ? 0 : 1, parent->status, http_status_name (parent->status));
  httpd_write_common_headers (parent, buf);
  if (http->sz && (parent->peer_flags & HIN_HTTP_CHUNKED) == 0)
    header (buf, "Content-Length: %ld\r\n", http->sz);

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
    if (matchi_string (&line, "Content-Length:") > 0) {
    } else if (matchi_string (&line, "Transfer-Encoding:") > 0) {
    } else if (matchi_string (&line, "Content-Encoding:") > 0) {
    } else if (matchi_string (&line, "Connection:") > 0) {
    } else if (matchi_string (&line, "Cache-Control:") > 0) {
    } else if (matchi_string (&line, "Server:") > 0) {
    } else if (matchi_string (&line, "Date:") > 0) {
    } else if (matchi_string (&line, "Accept:") > 0) {
    } else if (matchi_string (&line, "Accept-Encoding:") > 0) {
    } else if (matchi_string (&line, "Accept-Ranges:") > 0) {
    } else {
      header (buf, "%.*s\r\n", line.len, line.ptr);
    }
  }

  header (buf, "\r\n");

  if (http->debug & DEBUG_RW) printf ("httpd %d proxy response %d '\n%.*s'\n", parent->c.sockfd, buf->count, buf->count, buf->ptr);

  hin_pipe_prepend_raw (pipe, buf);

  return 0;
}

static int hin_rproxy_finish (http_client_t * http, hin_pipe_t * pipe) {
  httpd_client_t * parent = http->c.parent;
  http->save_fd = 0;
  parent->state &= ~(HIN_REQ_PROXY | HIN_REQ_DATA);
  httpd_client_finish_request (parent, NULL);
  return 0;
}

static int hin_rproxy_post_done_callback (hin_pipe_t * pipe) {
  http_client_t * http = pipe->parent;

  if (pipe->debug & (DEBUG_POST|DEBUG_PIPE))
    printf ("pipe %d>%d post done %lld\n", pipe->in.fd, pipe->out.fd, (long long)pipe->count);

  http->io_state &= ~HIN_REQ_POST;
  int http_client_finish_request (http_client_t * http);
  http_client_finish_request (http);

  return 0;
}

static int hin_rproxy_send_post (http_client_t * http, off_t sz) {
  httpd_client_t * parent = http->c.parent;
  http->io_state |= HIN_REQ_POST;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = parent->c.sockfd;
  pipe->in.flags = HIN_SOCKET | (parent->c.flags & HIN_SSL);
  pipe->in.ssl = &parent->c.ssl;
  pipe->out.fd = http->c.sockfd;
  pipe->out.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->parent = http;
  pipe->finish_callback = hin_rproxy_post_done_callback;
  pipe->debug = http->debug;

  pipe->in.flags |= HIN_COUNT;
  pipe->left = pipe->sz = sz;

  hin_pipe_start (pipe);
  return 0;
}

static int hin_rproxy_send (http_client_t * http, hin_buffer_t * buf) {
  httpd_client_t * parent = http->c.parent;
  string_t source, line, orig;
  source = parent->headers;

  off_t sz = 0;

  find_line (&source, &line);
  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
    orig = line;
    if (matchi_string (&line, "Host:") > 0) {
    } else if (matchi_string (&line, "Content-Length:") > 0) {
      match_string (&line, "%s*");
      sz = atoi (line.ptr);
    } else if (matchi_string (&line, "Connection:") > 0) {
    } else if (matchi_string (&line, "Accept-Encoding:") > 0) {
    } else if (matchi_string (&line, "Transfer-Encoding:") > 0) {
    } else if (matchi_string (&line, "Content-Encoding:") > 0) {
    } else {
      header (buf, "%.*s\r\n", orig.len, orig.ptr);
    }
  }

  if (sz > 0) {
    header (buf, "Content-Length: %ld\r\n", sz);
    header (buf, "\r\n");
    if (source.len > sz) source.len = sz;
    header_raw (buf, source.ptr, source.len);
    hin_rproxy_send_post (http, sz - source.len);
    return 1;
  }

  return 0;
}

static int hin_rproxy_state_callback (http_client_t * http, uint32_t state, uintptr_t data) {
  switch (state) {
  case HIN_HTTP_STATE_SEND:
    return hin_rproxy_send (http, (hin_buffer_t*)data);
  break;
  case HIN_HTTP_STATE_HEADERS:
    return hin_rproxy_headers (http, (hin_pipe_t*)data);
  break;
  case HIN_HTTP_STATE_FINISH:
    return hin_rproxy_finish (http, (hin_pipe_t*)data);
  break;
  case HIN_HTTP_STATE_CONNECTED:
  break;
  default:
    printf ("http %d rproxy unhandled state %x\n", http->c.sockfd, state);
    return hin_rproxy_finish (http, NULL);
  break;
  }
  return 0;
}

http_client_t * hin_proxy (httpd_client_t * parent, http_client_t * http, const char * url) {
  if (parent->state & HIN_REQ_DATA) return NULL;
  parent->state |= HIN_REQ_DATA | HIN_REQ_PROXY;

  int hin_cache_check (void * store, httpd_client_t * client);
  if (hin_cache_check (NULL, parent) > 0) {
    return 0;
  }

  if (http == NULL) {
    http = http_connection_get (url);
  }

  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    http->flags |= HIN_HTTP_KEEPALIVE;
  }

  http->c.parent = parent;
  http->debug = parent->debug;
  http->method = parent->method;

  http->state_callback = hin_rproxy_state_callback;

  http_connection_start (http);

  return http;
}


