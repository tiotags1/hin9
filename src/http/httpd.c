
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "lua.h"

void httpd_client_clean (httpd_client_t * http) {
  if (http->file_path) free ((void*)http->file_path);
  if (http->post_sep) free ((void*)http->post_sep);
  if (http->post_fd) close (http->post_fd);
  memset (http, 0, sizeof (*http));
}

int httpd_client_start_request (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("httpd request begin\n");
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  http->state |= HIN_HEADERS;

  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_data_t * data = (hin_server_data_t*)server->parent;
  if (data) {
    httpd_client_t * http = (httpd_client_t*)&client->extra;
    http->disable = data->disable;
  }
}

int httpd_client_reread (hin_client_t * client);

int httpd_client_finish (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  if ((http->state & (HIN_SERVICE|HIN_POST|HIN_WAIT)) == 0) {
    if ((http->flags & HIN_HTTP_KEEP)) {
      httpd_client_clean (http);
      httpd_client_start_request (client);
      httpd_client_reread (client);
    } else {
      hin_client_shutdown (client);
      hin_lines_request (client->read_buffer);
      httpd_client_clean (http);
      client->read_buffer = NULL;
      http->state |= HIN_END;
    }
  }
}

int httpd_client_finish_request (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("httpd request done\n");
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  http->state = (http->state & ~HIN_SERVICE);
  httpd_client_finish (client);
}

int httpd_client_error (hin_client_t * client) {
  printf ("httpd error !!!\n");
}

int httpd_client_close (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("httpd close client\n");
}

int httpd_client_read_callback (hin_client_t * client, string_t * source);

int httpd_client_reread (hin_client_t * client) {
  hin_buffer_t * buffer = client->read_buffer;

  string_t source;
  source.ptr = buffer->data;
  source.len = buffer->ptr - buffer->data;
  int num = 0;
  if (source.len > 0)
    num = httpd_client_read_callback (client, &source);
  if (num < 0) {
    printf ("client error\n");
    hin_client_shutdown (client);
    return -1;
  } else if (num > 0) {
    hin_buffer_eat (buffer, num);
  } else {
    buffer->count = buffer->sz;
    hin_lines_request (buffer);
  }
}

#include <sys/stat.h>
#include <fcntl.h>
int post_done (hin_pipe_t * pipe) {
  printf ("post done %d\n", pipe->out.fd);
  //close (pipe->out);
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)client->extra;
  http->state &= ~HIN_POST;
  httpd_client_finish (client);
}

int httpd_client_read_callback (hin_client_t * client, string_t * source) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;

  if (source->len > 65000) {
    httpd_respond_error (client, 413, NULL);
    hin_client_shutdown (client);
    return -1;
  }

  int used = httpd_parse_req (client, source);
  if (used <= 0) return used;
  if (http->post_sz > 0) {
    int consume = source->len > http->post_sz ? http->post_sz : source->len;
    off_t left = http->post_sz - consume;
    used += consume;
    // send post data in buffer to post handler
    http->post_fd =  openat (AT_FDCWD, "/tmp/upload.txt", O_RDWR | O_CREAT | O_TRUNC, 0600);
    printf ("post initial %ld %ld fd %d '%.*s'\n", http->post_sz, source->len, http->post_fd, consume, source->ptr);
    write (http->post_fd, source->ptr, consume);
    if (left > 0) {
      http->state |= HIN_POST;
      // request more post
      receive_file (client, http->post_fd, consume, left, 0, post_done);
      //return used;
    } else {
      //close (http->post_fd);
    }
  }
  http->status = 200;
  int hin_server_callback (hin_client_t * client);
  hin_server_callback (client);

  if (http->flags & HIN_HTTP_CAN_DEFLATE) {
    int hin_client_deflate_init (httpd_client_t * http);
    hin_client_deflate_init (http);
  }

  if ((http->state & ~(HIN_HEADERS|HIN_END)) == 0) {
    printf ("httpd 500 missing request\n");
    httpd_respond_error (client, 500, NULL);
    hin_client_shutdown (client);
    return used;
  }
  if (http->state & HIN_HEADERS) {
    httpd_client_reread (client);
  }
  return used;
}

int httpd_client_accept (hin_client_t * client) {
  httpd_client_start_request (client);
  client->read_buffer = hin_lines_create (client, client->sockfd, httpd_client_read_callback);
}

hin_client_t * httpd_create (const char * addr, const char * port, void * ssl_ctx) {
  hin_client_t * server = calloc (1, sizeof *server + sizeof (hin_server_blueprint_t));
  int sockfd = hin_socket_listen (addr, port, server);
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



