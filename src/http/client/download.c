
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

int http_client_start_headers (http_client_t * http, int ret);

int hin_http_state (http_client_t * http, int state, uintptr_t data) {
  if (http->debug & DEBUG_HTTP)
    printf ("http %d state is %d\n", http->c.sockfd, state);

  string_t url = http->uri.all;
  switch (state) {
  case HIN_HTTP_STATE_CONNECTED:
    http_client_start_headers (http, data);
  break;
  case HIN_HTTP_STATE_SSL_FAILED: // fall-through
  case HIN_HTTP_STATE_CONNECTION_FAILED:
    if (http->debug & DEBUG_HTTP)
      fprintf (stderr, "%.*s connection failed: %s\n", (int)url.len, url.ptr, strerror (-data));
  break;
  case HIN_HTTP_STATE_HEADERS_FAILED:
    if (http->debug & DEBUG_HTTP)
      fprintf (stderr, "%.*s failed to download headers\n", (int)url.len, url.ptr);
  break;
  case HIN_HTTP_STATE_ERROR:
    if (http->debug & DEBUG_HTTP)
      fprintf (stderr, "%.*s generic error\n", (int)url.len, url.ptr);
  break;
  }
  if (http->state_callback) {
    http->state_callback (http, state, data);
  }
  return 0;
}

void http_client_unlink (http_client_t * http);

static int connected (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;

  http->c.sockfd = ret;

  if (ret < 0) {
    hin_http_state (http, HIN_HTTP_STATE_CONNECTION_FAILED, ret);
    http_client_unlink (http);
    return 0;
  }

  if (http->uri.https) {
    if (hin_ssl_connect_init (&http->c) < 0) {
      hin_http_state (http, HIN_HTTP_STATE_SSL_FAILED, -EPROTO);
      http_client_unlink (http);
      return 0;
    }
  }

  hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  http->read_buffer = buf;
  if (hin_request_read (buf) < 0) {
    return 0;
  }

  hin_http_state (http, HIN_HTTP_STATE_CONNECTED, (uintptr_t)buf);
  return 0;
}

static int state_callback (http_client_t * http, uint32_t state, uintptr_t data) {
  return 0;
}

static int read_callback (hin_pipe_t * pipe, hin_buffer_t * buf, int num, int flush) {
  if (num <= 0) return 1;
  buf->count = num;
  hin_pipe_append_raw (pipe, buf);

  http_client_t * http = pipe->parent;
  if (num == 0 && flush != 0 && http->sz) {
  }

  int download_progress (http_client_t * http, hin_pipe_t * pipe, int num, int flush);
  if (http->debug & DEBUG_PROGRESS) {
    download_progress (http, pipe, num, flush);
  }
  //if (flush) return 1; // already cleaned in the write done handler
  return 0;
}

http_client_t * http_download_raw (http_client_t * http, const char * url1) {
  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    fprintf (stderr, "can't parse uri '%s'\n", url1);
    free (url);
    return NULL;
  }

  if (http == NULL) {
    http = calloc (1, sizeof (*http));
    http->debug = master.debug;
  }
  http->uri = info;
  http->c.sockfd = -1;
  http->c.magic = HIN_CONNECT_MAGIC;
  http->c.ai_addrlen = sizeof (http->c.ai_addr);

  http->flags |= HIN_HTTP_KEEPALIVE;

  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }

  http->read_callback = read_callback;
  http->state_callback = state_callback;

  hin_connect (http->host, http->port, &connected, http, &http->c.ai_addr, &http->c.ai_addrlen);
  http_connection_allocate (http);

  return http;
}

static int hin_rproxy_headers (http_client_t * http, hin_pipe_t * pipe) {
  httpd_client_t * parent = http->c.parent;
  pipe->out.fd = parent->c.sockfd;
  pipe->out.flags &= ~(HIN_FILE | HIN_OFFSETS);

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

static int hin_rproxy_read_callback (hin_pipe_t * pipe, hin_buffer_t * buf, int num, int flush) {
  if (num <= 0) return 1;
  buf->count = num;
  hin_pipe_append_raw (pipe, buf);

  //if (flush) return 1; // already cleaned in the write done handler
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
  http->c.sockfd = -1;
  http->c.magic = HIN_CONNECT_MAGIC;
  http->c.ai_addrlen = sizeof (http->c.ai_addr);

  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }

  http->flags |= HIN_HTTP_KEEPALIVE;
  http->save_fd = parent->c.sockfd;

  http->read_callback = hin_rproxy_read_callback;
  http->state_callback = hin_rproxy_state_callback;

  hin_connect (http->host, http->port, &connected, http, &http->c.ai_addr, &http->c.ai_addrlen);
  http_connection_allocate (http);

  return http;
}


