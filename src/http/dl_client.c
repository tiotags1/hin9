
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

static int http_client_close_callback (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;
  if (ret < 0) {
    printf ("http client close callback error: %s\n", strerror (-ret));
    return -1;
  }
  if (master.debug & DEBUG_PROTO) printf ("http close client %d\n", http->c.sockfd);
  free (http);
  return 1;
}

int http_client_shutdown (http_client_t * http) {
  if (master.debug & DEBUG_SOCKET) printf ("socket shutdown %d\n", http->c.sockfd);
  hin_buffer_t * buf = malloc (sizeof *buf);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = http_client_close_callback;
  buf->parent = &http->c;
  buf->ssl = &http->c.ssl;
  hin_request_close (buf);
}

void http_client_clean (http_client_t * http) {
  if (http->save_path) free ((void*)http->save_path);
  if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
  memset (http, 0, sizeof (*http));
}

int http_client_start_request (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client;
  if (ret < 0) {
    printf ("can't connect '%s'\n", strerror (-ret));
    return -1;
  }
  if (master.debug & DEBUG_PROTO) printf ("http request begin on socket %d\n", ret);
  //
  int http_send_request (hin_client_t * client);
  http_send_request (client);
}

int http_client_finish_request (http_client_t * http) {
  if (master.debug & DEBUG_PROTO) printf ("http request done\n");
  http_client_shutdown (http);
}

static int connected (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client;
  if (ret < 0) {
    return -1;
  }
  if (http->uri.https) {
    hin_connect_ssl_init (client);
  }
  http_client_start_request (client, ret);
}

hin_client_t * http_download (const char * url1, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush)) {
  printf ("http download '%s' to '%s'\n", url1, save_path);
  char * url = strdup (url1);
  hin_uri_t info;
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url1);
    return NULL;
  }

  char * h = strndup (info.host.ptr, info.host.len);
  char * p = NULL;
  if (info.port.len > 0) {
    p = strndup (info.port.ptr, info.port.len);
  } else {
    p = strdup ("80");
  }
  hin_client_t * client = calloc (1, sizeof (http_client_t));
  int ret = hin_connect (client, h, p, &connected);
  if (ret < 0) { printf ("can't create connection\n"); return NULL; }
  free (h);
  free (p);
  http_client_t * http = (http_client_t*)client;
  http->save_path = strdup (save_path);
  http->uri = info;
  return client;
}



