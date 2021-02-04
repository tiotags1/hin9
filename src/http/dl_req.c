
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

static int hin_http_pipe_finish_callback (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("download file transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  http_client_t * http = (http_client_t*)pipe->parent;
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    hin_client_list_add (&master.connection_list, &http->c);
  } else {
    http_client_shutdown (http);
  }
  int http_client_finish_request (http_client_t * http);
  http_client_finish_request (http);
}

int http_client_send_data (hin_client_t * client, string_t * source) {
  http_client_t * http = (http_client_t*)client;

  off_t len = source->len;
  off_t sz = http->sz;
  if (sz && sz < len) {
    len = sz;
  }

  const char * file_path = http->save_path;
  http->save_fd = open (file_path, O_RDWR | O_CLOEXEC | O_TRUNC | O_CREAT, 0666);
  if (http->save_fd < 0) {
    perror ("httpd open");
    return -1;
  }

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = client->sockfd;
  pipe->in.flags = HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->in.ssl = &client->ssl;
  pipe->in.pos = 0;
  pipe->out.fd = http->save_fd;
  pipe->out.flags = HIN_FILE | HIN_OFFSETS;
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->finish_callback = hin_http_pipe_finish_callback;

  if (http->flags & HIN_HTTP_CHUNKED) {
    int hin_pipe_decode_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
    pipe->decode_callback = hin_pipe_decode_chunked;
  } else if (sz > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->count = pipe->sz = sz;
  }

  if (len > 0) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
    hin_pipe_append (pipe, buf1);
  }

  hin_pipe_start (pipe);

  source->ptr += len;
  source->len -= len;

  return len;
}

int http_client_headers_read_callback (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  http_client_t * http = (http_client_t*)client;
  string_t data;
  data.ptr = buffer->data;
  data.len = buffer->ptr - buffer->data;

  int http_parse_headers (hin_client_t * client, string_t * source);
  int used = http_parse_headers (client, &data);

  if (used > 0) http_client_send_data (client, &data);

  return used;
}

static int http_client_sent_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) {
    printf ("http header send failed %s\n", strerror (-ret));
    http_client_shutdown (buffer->parent);
    return 1;
  }
  printf ("sent done\n");
  return 1;
}

int http_send_request (hin_client_t * client) {
  http_client_t * http = (http_client_t*)client;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = http_client_sent_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;

  char * path = http->uri.path.ptr;
  char * path_max = path + http->uri.path.len;
  if (http->uri.query.len > 0) {
    path_max = http->uri.query.ptr + http->uri.query.len;
  }

  header (client, buf, "GET %.*s HTTP/1.1\r\n", path_max - path, path);
  if (http->uri.port.len > 0) {
    header (client, buf, "Host: %.*s:%.*s\r\n", http->uri.host.len, http->uri.host.ptr, http->uri.port.len, http->uri.port.ptr);
  } else {
    header (client, buf, "Host: %.*s\r\n", http->uri.host.len, http->uri.host.ptr);
  }
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    header (client, buf, "Connection: keep-alive\r\n");
  } else {
    header (client, buf, "Connection: close\r\n");
  }
  header (client, buf, "\r\n");
  if (master.debug & DEBUG_RW) printf ("http request '%.*s'\n", buf->count, buf->ptr);
  hin_request_write (buf);
}


