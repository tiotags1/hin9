
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

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
  string_t source, line;
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

  header (buf, "HTTP/1.%d %d %s\r\n", parent->peer_flags & HIN_HTTP_VER0 ? 0 : 1, parent->status, http_status_name (parent->status));
  httpd_write_common_headers (parent, buf);
  if (http->sz && (parent->peer_flags & HIN_HTTP_CHUNKED) == 0)
    header (buf, "Content-Length: %ld\r\n", http->sz);

  find_line (&source, &line);
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

static int hin_rproxy_state_callback (http_client_t * http, uint32_t state, uintptr_t data) {
  switch (state) {
  case HIN_HTTP_STATE_HEADERS:
    return hin_rproxy_headers (http, (hin_pipe_t*)data);
  break;
  case HIN_HTTP_STATE_FINISH:
    return hin_rproxy_finish (http, (hin_pipe_t*)data);
  break;
  }
  return 0;
}

http_client_t * hin_proxy (httpd_client_t * parent, http_client_t * http, const char * url1) {
  if (parent->state & HIN_REQ_DATA) return NULL;
  parent->state |= HIN_REQ_DATA | HIN_REQ_PROXY;

  int hin_cache_check (void * store, httpd_client_t * client);
  if (hin_cache_check (NULL, parent) > 0) {
    return 0;
  }

  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    fprintf (stderr, "can't parse uri '%s'\n", url);
    free (url);
    return NULL;
  }

  if (http == NULL) {
    http = calloc (1, sizeof (*http));
    http->debug = master.debug;
  }
  http->uri = info;
  http->c.parent = parent;
  http->debug = parent->debug;

  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }

  http->save_fd = parent->c.sockfd;

  http->state_callback = hin_rproxy_state_callback;

  hin_http_connect_start (http);

  return http;
}


