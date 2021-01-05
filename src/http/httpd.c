
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "lua.h"

void httpd_client_ping (hin_client_t * client, int timeout);
int httpd_client_reread (hin_client_t * client);
int httpd_client_read_callback (hin_client_t * client, hin_buffer_t * buffer);

void httpd_client_clean (httpd_client_t * http) {
  if (http->file_path) free ((void*)http->file_path);
  if (http->post_sep) free ((void*)http->post_sep);
  if (http->post_fd) close (http->post_fd);
  if (http->append_headers) free (http->append_headers);
  memset (http, 0, sizeof (*http));
}

int httpd_client_start_request (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("httpd request begin\n");
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  http->state |= HIN_REQ_HEADERS;

  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_data_t * data = (hin_server_data_t*)server->parent;
  if (data) {
    httpd_client_t * http = (httpd_client_t*)&client->extra;
    http->disable = data->disable;
  }
  httpd_client_ping (client, data->timeout);
}

int httpd_client_finish (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  if ((http->state & (HIN_REQ_DATA | HIN_REQ_POST | HIN_REQ_WAIT)) == 0) {
    if ((http->peer_flags & HIN_HTTP_KEEPALIVE)) {
      httpd_client_clean (http);
      httpd_client_start_request (client);
      httpd_client_reread (client);
    } else {
      hin_client_shutdown (client);
      hin_lines_request (client->read_buffer);
      httpd_client_clean (http);
      client->read_buffer = NULL;
      http->state |= HIN_REQ_END;
    }
  }
}

int httpd_client_finish_request (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("httpd request done\n");
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  http->state = (http->state & ~HIN_REQ_DATA);
  httpd_client_finish (client);
}

int httpd_client_error (hin_client_t * client) {
  printf ("httpd error !!!\n");
}

int httpd_client_shutdown (hin_client_t * client) {
  printf ("httpd reqest shutdown\n");
  hin_client_shutdown (client);
}

int httpd_client_close (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("httpd close client\n");
}

int httpd_client_accept (hin_client_t * client) {
  httpd_client_start_request (client);
  client->read_buffer = hin_lines_create (client, client->sockfd, httpd_client_read_callback);
}

hin_client_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx) {
  hin_client_t * server = calloc (1, sizeof *server + sizeof (hin_server_blueprint_t));
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
  hin_server_blueprint_t * bp = (hin_server_blueprint_t *)&server->extra;
  bp->client_close = httpd_client_close;
  bp->client_error = httpd_client_error;
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



