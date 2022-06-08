
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin/hin.h"
#include "hin/listen.h"
#include "hin/http/http.h"

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
  }
  return 0;
}

int httpd_client_accept (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)client;
  hin_server_t * server = http->c.parent;
  http->debug = server->debug;

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
}

hin_server_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx) {
  hin_server_t * server = calloc (1, sizeof (hin_server_t));

  server->accept_callback = httpd_client_accept;
  //server->sni_callback = httpd_client_sni_callback;
  server->user_data_size = sizeof (httpd_client_t);
  server->ssl_ctx = ssl_ctx;
  server->accept_flags = SOCK_CLOEXEC;
  server->debug = master.debug;

  if (master.debug & (DEBUG_BASIC|DEBUG_SOCKET))
    printf ("http%sd listening on '%s':'%s'\n", ssl_ctx ? "s" : "", addr ? addr : "all", port);

  int err = hin_request_listen (server, addr, port, sock_type);
  if (err < 0) {
    printf ("error requesting socket\n");
    // free socket
    return NULL;
  }

  return server;
}


