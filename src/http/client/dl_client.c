
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

int http_client_headers_read_callback (hin_buffer_t * buffer, int received);

int http_connection_allocate (http_client_t * http) {
  if (http->io_state & HIN_REQ_IDLE)
    hin_client_list_remove (&master.connection_list, &http->c);
  http->io_state &= ~HIN_REQ_IDLE;
  return 0;
}

int http_connection_release (http_client_t * http) {
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    http->io_state |= HIN_REQ_IDLE;
    hin_client_list_add (&master.connection_list, &http->c);
  } else {
    http_client_shutdown (http);
  }
  return 0;
}

void http_client_clean (http_client_t * http) {
  if (http->debug & DEBUG_MEMORY) printf ("http %d clean\n", http->c.sockfd);
  if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = http->port = NULL;
  if (http->save_fd) { // TODO should it clean save_fd ?
    if (http->debug & DEBUG_SYSCALL) printf ("  close save_fd %d\n", http->save_fd);
    close (http->save_fd);
    http->save_fd = 0;
  }
  http->io_state &= (~HIN_REQ_END);
}

void http_client_unlink (http_client_t * http) {
  if (http->debug & DEBUG_HTTP) printf ("http %d unlink\n", http->c.sockfd);

  master.num_connection--;

  http_client_clean (http);
  if (http->read_buffer) {
    hin_buffer_clean (http->read_buffer);
  }
  free (http);
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
  if (http->io_state & HIN_REQ_END) return 0;
  http->io_state |= HIN_REQ_END;

  if (http->io_state & HIN_REQ_IDLE)
    hin_client_list_remove (&master.connection_list, &http->c);

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

  return 0;
}

int http_client_buffer_close (hin_buffer_t * buf, int ret) {
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
  http_client_t * next = NULL;
  for (http_client_t * elem = (http_client_t*)master.connection_list; elem; elem = next) {
    next = (http_client_t *)elem->c.next;
    http_client_shutdown (elem);
  }
}

int http_client_finish_request (http_client_t * http) {
  if (http->debug & DEBUG_HTTP) printf ("http %d request done\n", http->c.sockfd);

  http_connection_release (http);

  if ((http->flags & HIN_HTTP_KEEPALIVE) == 0) {
    http_client_shutdown (http);
    return 0;
  }
  http->c.parent = NULL;
  if (hin_request_read (http->read_buffer) < 0) {
    http_client_shutdown (http);
  }
  return 0;
}

static int connected (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;

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

  hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
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


