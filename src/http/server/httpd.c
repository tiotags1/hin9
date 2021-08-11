
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <basic_vfs.h>

#include "hin.h"
#include "http.h"
#include "vhost.h"
#include "conf.h"

int httpd_client_reread (httpd_client_t * http);

void httpd_client_clean (httpd_client_t * http) {
  if (http->debug & (DEBUG_HTTP|DEBUG_MEMORY)) printf ("httpd %d clean\n", http->c.sockfd);
  if (http->file_path) free ((void*)http->file_path);
  if (http->post_sep) free ((void*)http->post_sep);

  if (http->file_fd) {
    if (http->debug & (DEBUG_HTTP|DEBUG_SYSCALL)) printf ("  close file_fd %d\n", http->file_fd);
    close (http->file_fd);
  }
  if (!HIN_HTTPD_WORKER_PREFORKED && http->post_fd) {
    if (http->debug & (DEBUG_HTTP|DEBUG_SYSCALL)) printf ("  close post_fd %d\n", http->post_fd);
    close (http->post_fd);
  }
  http->file_fd = http->post_fd = 0;

  if (http->append_headers) free (http->append_headers);
  if (http->content_type) free (http->content_type);
  if (http->hostname) free (http->hostname);

  if (http->peer_flags & HIN_HTTP_COMPRESS) {
    int ret = deflateEnd (&http->z);
    if (ret != Z_OK) {
      printf ("deflate end failed\n");
    }
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
  hin_timer_remove (&http->timer);
}

int httpd_client_start_request (httpd_client_t * http) {
  http->state = HIN_REQ_HEADERS | (http->state & HIN_REQ_ENDING);

  hin_server_t * server = (hin_server_t*)http->c.parent;
  hin_vhost_t * vhost = (hin_vhost_t*)server->c.parent;
  httpd_vhost_switch (http, vhost);

  if (http->debug & DEBUG_HTTP) {
    printf ("http%sd %d request begin %lld\n", (http->c.flags & HIN_SSL) ? "s" : "",
      http->c.sockfd, (long long)time (NULL));
  }
  return 0;
}

int httpd_client_finish_request (httpd_client_t * http) {
  if (http->c.type == HIN_CACHE_OBJECT) {
    printf ("httpd client finished error: received cache object\n");
  }
  // it waits for post data to finish
  http->state &= ~HIN_REQ_DATA;
  if (http->state & (HIN_REQ_POST)) return 0;

  int keep = (http->peer_flags & HIN_HTTP_KEEPALIVE) && ((http->state & HIN_REQ_ENDING) == 0);
  if (http->debug & DEBUG_HTTP) printf ("httpd %d request done %s\n", http->c.sockfd, keep ? "keep" : "close");

  int hin_server_finish_callback (httpd_client_t * client);
  hin_server_finish_callback (http);

  if (http->read_buffer)
    hin_buffer_eat (http->read_buffer, http->headers.len);

  if (http->file) {
    extern basic_vfs_t * vfs;
    basic_vfs_unref (vfs, http->file);
  }

  httpd_client_clean (http);
  if (keep) {
    httpd_client_start_request (http);
    httpd_client_reread (http);
  } else {
    http->state |= HIN_REQ_END;
    httpd_client_shutdown (http);
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
    hin_buffer_clean (http->read_buffer);
    http->read_buffer = NULL;
  }
  httpd_client_clean (http);
  hin_client_unlink (&http->c);
  return 1;
}

int httpd_client_shutdown (httpd_client_t * http) {
  if (http->state & HIN_REQ_ENDING) return -1;
  http->state |= HIN_REQ_ENDING;
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

static int httpd_client_buffer_close_callback (hin_buffer_t * buffer, int ret) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;
  if (http->debug & DEBUG_HTTP) printf ("httpd %d shutdown buffer %s\n", http->c.sockfd, ret < 0 ? strerror (-ret) : "");
  httpd_client_shutdown (http);
  return 0;
}

static int httpd_client_buffer_eat_callback (hin_buffer_t * buffer, int num) {
  if (num > 0) {
  } else if (num == 0) {
    hin_lines_request (buffer, 0);
  } else {
    printf ("httpd eat callback error\n");
  }
  return 0;
}

#if HIN_HTTPD_NULL_SERVER
extern hin_buffer_t * buf_list;

int temp_callback2 (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    printf ("null server %d error write '%s'\n", buf->fd, strerror (-ret));
  }
  httpd_client_t * http = buf->parent;
  //close (http->c.sockfd);
  httpd_client_shutdown (http);
  hin_buffer_list_add (&buf_list, buf);
  return 0;
}

int temp_callback1 (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    printf ("null server %d error read '%s'\n", buf->fd, strerror (-ret));
  }

  buf->callback = temp_callback2;
  buf->count = 0;

  const char * text = "HTTP/1.0 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\nHello\n";
  header_raw (buf, text, strlen (text));
  hin_request_write_fixed (buf);

  return 0;
}
#endif

int httpd_client_accept (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)client;

  hin_timer_t * timer = &http->timer;
  int httpd_timeout_callback (hin_timer_t * timer, time_t time);
  timer->callback = httpd_timeout_callback;
  timer->ptr = http;

#if HIN_HTTPD_NULL_SERVER
  hin_buffer_t * buf = buf_list;
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->callback = temp_callback1;
  buf->count = buf->sz;
  buf->debug = http->debug;

  hin_buffer_list_remove (&buf_list, buf);
  hin_request_read_fixed (buf);

  return 0;
#else
  httpd_client_start_request (http);

  hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;
  http->read_buffer = buf;

  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  int httpd_client_read_callback (hin_buffer_t * buffer, int received);
  lines->read_callback = httpd_client_read_callback;
  lines->close_callback = httpd_client_buffer_close_callback;
  lines->eat_callback = httpd_client_buffer_eat_callback;
  hin_lines_request (buf, 0);
  return 0;
#endif
}

hin_server_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx) {
  hin_server_t * server = calloc (1, sizeof (hin_server_t));

  server->c.sockfd = -1;
  server->c.type = HIN_SERVER;
  server->c.magic = HIN_SERVER_MAGIC;
  server->client_handle = httpd_client_accept;
  server->user_data_size = sizeof (httpd_client_t);
  server->ssl_ctx = ssl_ctx;
  server->accept_flags = SOCK_CLOEXEC;
  server->debug = master.debug;

  if (master.debug & (DEBUG_BASIC|DEBUG_SOCKET))
    printf ("http%sd listening on '%s':'%s'\n", ssl_ctx ? "s" : "", addr ? addr : "all", port);

  hin_client_list_add (&master.server_list, &server->c);

  int err = hin_socket_request_listen (addr, port, sock_type, server);
  if (err < 0) {
    printf ("error requesting socket\n");
    // free socket
    return NULL;
  }

  return server;
}



