
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

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

  if (http->read_buffer == NULL) {
    hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
    buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
    buf->fd = http->c.sockfd;
    buf->parent = http;
    buf->ssl = &http->c.ssl;
    buf->debug = http->debug;
    http->read_buffer = buf;
  }

  if (hin_lines_request (http->read_buffer, 0)) {
    printf ("error! %d\n", 943547909);
    return 0;
  }

  hin_http_state (http, HIN_HTTP_STATE_CONNECTED, (uintptr_t)http->read_buffer);
  return 0;
}

int http_connection_start (http_client_t * http) {
  if (http->c.sockfd >= 0) {
    hin_http_state (http, HIN_HTTP_STATE_CONNECTED, (uintptr_t)http->read_buffer);

    hin_buffer_t * buf = http->read_buffer;
    buf->debug = http->debug;

    if (http->debug & DEBUG_HTTP)
      printf ("http %d reuse %s:%s\n", http->c.sockfd, http->host, http->port);

    if (hin_lines_request (buf, 0)) {
      printf ("error! %d\n", 953909);
      return 0;
    }

    return 0;
  }

  http->c.sockfd = -1;
  http->c.magic = HIN_CONNECT_MAGIC;
  http->c.ai_addrlen = sizeof (http->c.ai_addr);

  hin_connect (http->host, http->port, &connected, http, &http->c.ai_addr, &http->c.ai_addrlen);

  return 0;
}



