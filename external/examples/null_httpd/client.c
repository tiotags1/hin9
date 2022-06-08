
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin/hin.h"
#include "hin/conf.h"
#include "hin/http/http.h"

void httpd_client_clean (httpd_client_t * http) {
  if (http->debug & (DEBUG_HTTP|DEBUG_MEMORY)) printf ("httpd %d clean\n", http->c.sockfd);
  if (http->file_path) free ((void*)http->file_path);
  if (http->post_sep) free ((void*)http->post_sep);

  if (http->file_fd) {
    if (http->debug & (DEBUG_HTTP|DEBUG_SYSCALL)) printf ("  close file_fd %d\n", http->file_fd);
    close (http->file_fd);
  }
  http->file_fd = http->post_fd = 0;

  if (http->append_headers) free (http->append_headers);
  if (http->content_type) free (http->content_type);
  if (http->hostname) free (http->hostname);

  int ret = deflateEnd (&http->z);
  if (ret != Z_OK) {
  }

  http->peer_flags = http->disable = 0;
  http->status = http->method = 0;
  http->pos = http->count = 0;
  http->cache = http->modified_since = 0;
  http->cache_flags = 0;
  http->etag = http->post_sz = 0;
  http->post_sep = http->file_path = http->append_headers = NULL;
  http->hostname = http->content_type = NULL;
  http->file = NULL;
  http->headers.len = 0;
  http->headers.ptr = NULL;
}

int httpd_client_start_request (httpd_client_t * http) {
  http->state = HIN_REQ_HEADERS | (http->state & HIN_REQ_STOPPING);

  if (http->debug & DEBUG_HTTP) {
    printf ("http%sd %d request begin %lld\n", (http->c.flags & HIN_SSL) ? "s" : "",
      http->c.sockfd, (long long)time (NULL));
  }

  return 0;
}

static int httpd_client_close_callback (hin_buffer_t * buffer, int ret) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;
  if (ret < 0) {
    printf ("httpd %d client close callback error %s\n", http->c.sockfd, ret < 0 ? strerror (-ret) : "");
    return -1;
  }
  if (http->debug & (DEBUG_HTTP)) printf ("httpd %d close\n", http->c.sockfd);
  if (http->read_buffer && http->read_buffer != buffer) {
    hin_buffer_stop_clean (http->read_buffer);
    http->read_buffer = NULL;
  }
  httpd_client_clean (http);
  hin_client_close (&http->c);
  return 1;
}

int httpd_client_shutdown (httpd_client_t * http) {
  if (http->state & HIN_REQ_STOPPING) return -1;
  http->state |= HIN_REQ_STOPPING;
  if (http->debug & (DEBUG_HTTP)) printf ("httpd %d shutdown\n", http->c.sockfd);

  hin_buffer_t * buf = malloc (sizeof *buf);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = httpd_client_close_callback;
  buf->parent = (hin_client_t*)http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  if (hin_request_close (buf) < 0) {
    buf->flags |= HIN_SYNC;
    hin_request_close (buf);
  }
  return 0;
}


