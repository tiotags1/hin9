
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

void http_client_clean (http_client_t * http) {
  hin_client_list_remove (&master.connection_list, (hin_client_t*)http);
  master.num_connection--;
  if (http->save_path) free ((void*)http->save_path);
  if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
  if (http->host) free (http->host);
  if (http->port) free (http->port);
  free (http);
}

static int http_client_close_callback (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;
  if (ret < 0) {
    printf ("http client close callback error: %s\n", strerror (-ret));
    return -1;
  }
  if (master.debug & DEBUG_PROTO) printf ("http close client %d\n", http->c.sockfd);
  http_client_clean (http);
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

int match_string_equal (string_t * source, const char * str) {
  const char * ptr = source->ptr;
  const char * max = ptr + source->len;
  while (1) {
    if (ptr >= max && *str == '\0') return 1;
    if (ptr >= max) return -1;
    if (*str == '\0') return -1;
    if (*ptr != *str) return -1;
    ptr++;
    str++;
  }
  return -1;
}

http_client_t * httpd_proxy_connection_get (string_t * host, string_t * port) {
  for (http_client_t * elem = (http_client_t*)master.connection_list; elem; elem = (http_client_t*)elem->c.next) {
    if (match_string_equal (host, elem->host) < 0) continue;
    if (match_string_equal (port, elem->port) < 0) continue;
    if (master.debug & DEBUG_PROXY) printf ("proxy reusing client %d\n", elem->c.sockfd);
    hin_client_list_remove (&master.connection_list, &elem->c);
    return elem;
  }
  return NULL;
}

void httpd_proxy_connection_close_all () {
  int http_client_shutdown (http_client_t * http);
  for (http_client_t * elem = (http_client_t*)master.connection_list; elem; elem = (http_client_t*)elem->c.next) {
    http_client_shutdown (elem);
  }
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

http_client_t * http_download (const char * url1, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush)) {
  printf ("http download '%s' to '%s'\n", url1, save_path);
  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url1);
    return NULL;
  }

  http_client_t * http = httpd_proxy_connection_get (&info.host, &info.port);
  if (http) {
    if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
    http->c.parent = NULL;
    http->uri = info;
    connected ((hin_client_t*)http, http->c.sockfd);
    return http;
  }

  http = calloc (1, sizeof (http_client_t));
  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }
  http->c.parent = NULL;
  http->uri = info;
  http->save_path = strdup (save_path);

  int ret = hin_connect (&http->c, http->host, http->port, &connected);
  if (ret < 0) { printf ("can't create connection\n"); return NULL; }
  master.num_connection++;

  return http;
}



