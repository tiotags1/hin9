
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <basic_vfs.h>

#include "hin.h"
#include "http.h"
#include "lua.h"
#include "conf.h"

void httpd_client_ping (httpd_client_t * http, int timeout);
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
    close (http->post_fd); // TODO cgi worker needs to keep this
  }
  http->file_fd = http->post_fd = 0;

  if (http->append_headers) free (http->append_headers);
  if (http->content_type) free (http->content_type);
  //memset (&http->state, 0, sizeof (httpd_client_t) - sizeof (hin_client_t)); // cleans things it shouldn't

  http->peer_flags = http->disable = 0;
  http->status = http->method = 0;
  http->pos = http->count = 0;
  http->cache = http->modified_since = 0;
  http->cache_flags = 0;
  http->etag = http->post_sz = 0;
  http->post_sep = http->file_path = http->append_headers = http->content_type = NULL;
  http->file = NULL;
}

int httpd_client_start_request (httpd_client_t * http) {
  http->state = HIN_REQ_HEADERS | (http->state & HIN_REQ_ENDING);

  hin_client_t * server = (hin_client_t*)http->c.parent;
  hin_server_data_t * data = (hin_server_data_t*)server->parent;
  if (data) {
    http->disable = data->disable;
    http->debug = data->debug;
  }

  if (http->debug & DEBUG_HTTP) printf ("httpd %d request begin\n", http->c.sockfd);
  httpd_client_ping (http, data->timeout);
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
    printf ("httpd %d client close callback error: %s\n", http->c.sockfd, strerror (-ret));
    return -1;
  }
  if (http->debug & (DEBUG_HTTP)) printf ("httpd %d close\n", http->c.sockfd);
  if (http->read_buffer && http->read_buffer != buffer) {
    hin_buffer_clean (http->read_buffer);
    http->read_buffer = NULL;
  }
  hin_buffer_clean (buffer);
  hin_client_unlink (&http->c);
  return 0;
}

int httpd_client_shutdown (httpd_client_t * http) {
  if (http->state & HIN_REQ_ENDING) return -1;
  http->state |= HIN_REQ_ENDING;
  if (http->debug & (DEBUG_HTTP)) printf ("httpd %d shutdown\n", http->c.sockfd);

#if 0
  struct linger sl;
  sl.l_onoff = 1;		// non-zero value enables linger option in kernel
  sl.l_linger = 10;		// timeout interval in seconds
  if (setsockopt (http->c.sockfd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl)) < 0)
    perror ("setsockopt(SO_LINGER) failed");
#endif

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

static int httpd_client_buffer_close_callback (hin_buffer_t * buffer) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;
  if (http->debug & DEBUG_HTTP) printf ("httpd %d shutdown buffer\n", http->c.sockfd);
  httpd_client_shutdown (http);
  return 0;
}

static int httpd_client_buffer_eat_callback (hin_buffer_t * buffer, int num) {
  if (num > 0) {
  } else if (num == 0) {
    hin_lines_request (buffer);
  } else {
    printf ("httpd eat callback error\n");
  }
  return 0;
}

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

int httpd_client_accept (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)client;
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

  hin_buffer_t * buf = hin_lines_create_raw ();
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;
  http->read_buffer = buf;

  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  int httpd_client_read_callback (hin_buffer_t * buffer);
  lines->read_callback = httpd_client_read_callback;
  lines->close_callback = httpd_client_buffer_close_callback;
  lines->eat_callback = httpd_client_buffer_eat_callback;
  if (hin_request_read (buf) < 0) {
    httpd_respond_fatal_and_full (http, 503, NULL);
    return -1;
  }
  return 0;
#endif
}

hin_client_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx) {
  hin_client_t * server = calloc (1, sizeof (hin_server_blueprint_t));
  int sockfd;
  sockfd = hin_socket_search (addr, port, sock_type, server);
  if (sockfd < 0) {
    sockfd = hin_socket_listen (addr, port, sock_type, server);
  } else {
    printf ("httpd server %d reuse sockfd\n", sockfd);
  }
  if (sockfd < 0) {
    printf ("httpd server %d can't listen on %s:%s\n", sockfd, addr, port);
    free (server);
    return NULL;
  }

  printf ("http%sd listening on '%s':'%s' sockfd %d\n", ssl_ctx ? "s" : "", addr ? addr : "all", port, sockfd);

  server->sockfd = sockfd;
  server->type = HIN_SERVER;
  server->magic = HIN_SERVER_MAGIC;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t *)server;
  bp->client_handle = httpd_client_accept;
  bp->user_data_size = sizeof (httpd_client_t);
  bp->ssl_ctx = ssl_ctx;
  bp->accept_flags = SOCK_CLOEXEC;

  hin_client_t * client = calloc (1, sizeof (hin_client_t) + bp->user_data_size);
  client->type = HIN_CLIENT;
  client->magic = HIN_CLIENT_MAGIC;
  client->parent = server;
  bp->accept_client = client;

  hin_buffer_t * buffer = calloc (1, sizeof *buffer);
  //buffer->sockfd = 0; // gets filled by accept
  buffer->parent = client;
  int hin_server_accept (hin_buffer_t * buffer, int ret);
  buffer->callback = hin_server_accept;
  bp->accept_buffer = buffer;
  if (hin_request_accept (buffer, bp->accept_flags) < 0) {
    printf ("conf error\n");
    exit (1);
  }

  hin_client_list_add (&master.server_list, server);

  return server;
}


