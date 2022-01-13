
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

int hin_http_connect_start (http_client_t * http) {
  http->c.sockfd = -1;
  http->c.magic = HIN_CONNECT_MAGIC;
  http->c.ai_addrlen = sizeof (http->c.ai_addr);

  hin_connect (http->host, http->port, &connected, http, &http->c.ai_addr, &http->c.ai_addrlen);
  http_connection_allocate (http);
  return 0;
}

http_client_t * http_download_raw (http_client_t * http, const char * url1) {
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

  hin_http_connect_start (http);

  return http;
}


