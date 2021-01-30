
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "lua.h"

void httpd_client_ping (httpd_client_t * http, int timeout);
int httpd_client_reread (httpd_client_t * http);

void httpd_client_clean (httpd_client_t * http) {
  if (http->file_path) free ((void*)http->file_path);
  if (http->post_sep) free ((void*)http->post_sep);
  if (http->file_fd) close (http->file_fd);
  if (http->post_fd) close (http->post_fd); // TODO cgi worker needs to keep this
  if (http->append_headers) free (http->append_headers);
  if (http->content_type) free (http->content_type);
  //memset (&http->state, 0, sizeof (httpd_client_t) - sizeof (hin_client_t)); // cleans things it shouldn't

  http->state = http->peer_flags = http->disable = 0;
  http->status = http->method = 0;
  http->file_fd = http->pos = http->count = 0;
  http->cache = http->modified_since = 0;
  http->etag = http->post_sz = 0;
  http->post_fd = 0;
  http->post_sep = http->file_path = http->append_headers = http->content_type = NULL;
}

int httpd_client_start_request (httpd_client_t * http) {
  if (master.debug & DEBUG_PROTO) printf ("httpd request begin\n");
  http->state = HIN_REQ_HEADERS;

  hin_client_t * server = (hin_client_t*)http->c.parent;
  hin_server_data_t * data = (hin_server_data_t*)server->parent;
  if (data) {
    http->disable = data->disable;
  }
  httpd_client_ping (http, data->timeout);
}

int httpd_client_finish_request (httpd_client_t * http) {
  if (master.debug & DEBUG_PROTO) printf ("httpd request done\n");

  // it waits for post data to finish
  if (http->state & HIN_REQ_POST) return 0;
  http->state &= ~HIN_REQ_DATA;

  if ((http->peer_flags & HIN_HTTP_KEEPALIVE)) {
    httpd_client_clean (http);
    httpd_client_start_request (http);
    httpd_client_reread (http);
  } else {
    httpd_client_clean (http);
    http->state |= HIN_REQ_END;
    httpd_client_shutdown (http);
  }
  return 0;
}

static int httpd_client_close_callback (hin_buffer_t * buffer, int ret) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;
  if (ret < 0) {
    printf ("httpd client close callback error: %s\n", strerror (-ret));
    return -1;
  }
  if (master.debug & DEBUG_PROTO) printf ("httpd close %d\n", http->c.sockfd);
  if (http->read_buffer && http->read_buffer != buffer) {
    hin_buffer_clean (http->read_buffer);
    http->read_buffer = NULL;
  }
  hin_buffer_clean (buffer);
  hin_client_unlink (&http->c);
  return 0;
}

int httpd_client_buffer_shutdown (hin_buffer_t * buffer) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;
  if (master.debug & DEBUG_PROTO) printf ("httpd shutdown buffer %d\n", http->c.sockfd);
  httpd_client_shutdown (http);
  return 0;
}

int httpd_client_shutdown (httpd_client_t * http) {
  if (http->state & HIN_REQ_ENDING) return -1;
  http->state |= HIN_REQ_ENDING;
  if (master.debug & DEBUG_SOCKET) printf ("httpd shutdown %d\n", http->c.sockfd);
  hin_buffer_t * buf = malloc (sizeof *buf);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = httpd_client_close_callback;
  buf->parent = (hin_client_t*)http;
  buf->ssl = &http->c.ssl;
  hin_request_close (buf);
  return 0;
}

int httpd_client_accept (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)client;
  httpd_client_start_request (http);

  hin_buffer_t * buf = hin_lines_create_raw ();
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->ssl = &http->c.ssl;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  int httpd_client_read_callback (hin_buffer_t * buffer);
  lines->read_callback = httpd_client_read_callback;
  lines->close_callback = httpd_client_buffer_shutdown;
  hin_request_read (buf);
  http->read_buffer = buf;
}

hin_client_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx) {
  hin_client_t * server = calloc (1, sizeof (hin_server_blueprint_t));
  int sockfd;
  sockfd = hin_socket_search (addr, port, sock_type, server);
  if (sockfd < 0) {
    sockfd = hin_socket_listen (addr, port, sock_type, server);
  } else {
    printf ("reused sockfd %d\n", sockfd);
  }
  if (sockfd < 0) {
    printf ("can't listen on %s:%s\n", addr, port);
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
  hin_request_accept (buffer, bp->accept_flags);

  hin_client_list_add (&master.server_list, server);

  return server;
}



