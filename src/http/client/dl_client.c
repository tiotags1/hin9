
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

int http_client_headers_read_callback (hin_buffer_t * buffer);

void http_client_clean (http_client_t * http) {
  if (http->debug & DEBUG_MEMORY) printf ("http %d clean\n", http->c.sockfd);
  if (http->save_path) free ((void*)http->save_path);
  if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = http->port = http->save_path = NULL;
  if (http->save_fd) {
    if (http->debug & DEBUG_SYSCALL) printf ("  close save_fd %d\n", http->save_fd);
    close (http->save_fd);
    http->save_fd = 0;
  }
}

void http_client_unlink (http_client_t * http) {
  if (http->debug & DEBUG_HTTP) printf ("http %d unlink\n", http->c.sockfd);
  hin_client_list_remove (&master.connection_list, (hin_client_t*)http);
  http_client_clean (http);
  if (http->read_buffer) {
    hin_buffer_clean (http->read_buffer);
  }
  free (http);
  master.num_connection--;
  hin_check_alive ();
}

static int http_client_close_callback (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;
  if (ret < 0) {
    printf ("http %d client close callback error: %s\n", http->c.sockfd, strerror (-ret));
    return -1;
  }
  hin_buffer_clean (buffer);
  http_client_unlink (http);
  return 0;
}

int http_client_shutdown (http_client_t * http) {
  if (http->debug & DEBUG_HTTP) printf ("http %d shutdown\n", http->c.sockfd);
  hin_buffer_t * buf = malloc (sizeof *buf);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = http_client_close_callback;
  buf->parent = &http->c;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  if (hin_request_close (buf) < 0) {
    buf->flags |= HIN_SYNC;
    hin_request_close (buf);
  }
  hin_client_list_remove (&master.connection_list, &http->c);
  return 0;
}

int http_client_buffer_close (hin_buffer_t * buf, int ret) {
  printf ("http %d buffer close %s\n", buf->fd, strerror (-ret));
  http_client_shutdown (buf->parent);
  return 0;
}

int match_string_equal1 (string_t * source, const char * str) {
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
    if (match_string_equal1 (host, elem->host) < 0) continue;
    if (match_string_equal1 (port, elem->port) < 0) continue;
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

int http_client_start_request (http_client_t * http, int ret) {
  if (ret < 0) {
    printf ("can't connect '%s'\n", strerror (-ret));
    return -1;
  }
  if (http->debug & DEBUG_HTTP) printf ("http %d request begin\n", ret);

  hin_lines_t * lines = (hin_lines_t*)&http->read_buffer->buffer;
  lines->read_callback = http_client_headers_read_callback;
  lines->close_callback = http_client_buffer_close;

  int http_send_request (http_client_t * http);
  http_send_request (http);
  return 0;
}

int http_client_finish_request (http_client_t * http) {
  if (http->debug & DEBUG_HTTP) printf ("http %d request done\n", http->c.sockfd);
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE && http->read_buffer) {
    http->c.parent = NULL;
    if (hin_request_read (http->read_buffer) < 0) {
      http_client_shutdown (http);
      return 0;
    }
    hin_client_list_add (&master.connection_list, (hin_client_t*)http);
  } else {
    http_client_shutdown (http);
  }
  return 0;
}

static int connected (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;

  master.num_connection++;
  int (*finish_callback) (http_client_t * http, int ret) = (void*)http->read_buffer;
  http->read_buffer = NULL;

  http->c.sockfd = ret;
  if (ret < 0) {
    finish_callback (http, ret);
    http_client_unlink (http);
    return 0;
  }

  if (http->uri.https) {
    if (hin_ssl_connect_init (&http->c) < 0) {
      printf ("http %d couldn't initialize ssl connection\n", http->c.sockfd);
      finish_callback (http, -EPROTO);
      http_client_unlink (http);
      return 0;
    }
  }

  hin_buffer_t * buf = hin_lines_create_raw ();
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  http->read_buffer = buf;
  if (hin_request_read (buf) < 0) {
    finish_callback (http, -EPROTO);
    http_client_unlink (http);
    return 0;
  }

  finish_callback (http, http->c.sockfd);
  return 0;
}

http_client_t * hin_http_connect (http_client_t * prev, string_t * host, string_t * port, int (*finish_callback) (http_client_t * http, int ret)) {
  http_client_t * http = httpd_proxy_connection_get (host, port);
  if (http) {
    if (prev->debug & DEBUG_PROXY) printf ("http %d reusing\n", http->c.sockfd);
    http_client_clean (http);
    void * rd = http->read_buffer;
    hin_client_t c = http->c;
    *http = *prev;
    http->read_buffer = rd;
    http->c = c;
    http->c.parent = prev->c.parent;
    http->debug = prev->debug;
    finish_callback (http, 0);
    free (prev);
    return http;
  }

  http = prev;
  if (http->read_buffer) printf ("http %d shouldn't be using read buffer\n", http->c.sockfd);
  http->read_buffer = (hin_buffer_t*)finish_callback;

  http->c.sockfd = -1;
  http->c.magic = HIN_CONNECT_MAGIC;
  http->c.ai_addrlen = sizeof (http->c.ai_addr);

  int ret = hin_connect (http->host, http->port, &connected, http, &http->c.ai_addr, &http->c.ai_addrlen);
  if (ret < 0) { printf ("http %d can't create connection\n", http->c.sockfd); return NULL; }

  return http;
}

http_client_t * http_download (const char * url1, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush)) {
  printf ("http download '%s' to '%s'\n", url1, save_path);
  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url1);
    return NULL;
  }

  http_client_t * http = calloc (1, sizeof (*http));
  http->c.parent = NULL;
  http->uri = info;
  http->save_path = strdup (save_path);
  http->debug = master.debug;

  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }

  http = hin_http_connect (http, &info.host, &info.port, http_client_start_request);

  return http;
}



