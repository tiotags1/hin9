
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

#include <sys/stat.h>
#include <fcntl.h>

int http_client_send_data (hin_client_t * client, string_t * source) {
  http_client_t * http = (http_client_t*)&client->extra;

  off_t len = source->len;
  if (http->sz < len) {
    len = http->sz;
  }
  const char * file_path = http->save_path;
  int filefd = open (file_path, O_RDWR | O_CLOEXEC | O_TRUNC | O_CREAT, 0666);
  if (filefd < 0) {
    perror ("httpd open");
    return -1;
  }
  int err = write (filefd, source->ptr, len);
  if (err < 0) {
    perror ("can't write ?\n");
  }
  off_t sz = http->sz;
  if (len > 0) { sz -= len; }
  receive_file (client, filefd, len, sz, 0, NULL);

  source->ptr += len;
  source->len -= len;

  return len;
}

int http_client_read_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) {
    printf ("error!! '%s'\n", strerror (-ret));
    return -1;
  }
  hin_client_t * client = (hin_client_t*)buffer->parent;
  http_client_t * http = (http_client_t*)&client->extra;
  string_t data;
  data.ptr = buffer->ptr;
  data.len = ret;
  int http_parse_headers (hin_client_t * client, string_t * source);
  http_parse_headers (client, &data);
  http_client_send_data (client, &data);
  return 0;
}

static int http_client_sent_callback (hin_buffer_t * buffer, int ret) {
  printf ("sent done\n");
  buffer->count = buffer->sz;
  buffer->callback = http_client_read_callback;
  hin_request_read (buffer);
  return 0;
}

int http_send_request (hin_client_t * client) {
  http_client_t * http = (http_client_t*)client->extra;

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

  header (client, buf, "GET %.*s HTTP/1.0\r\n", path_max - path, path);
  if (http->uri.port.len > 0) {
    header (client, buf, "Host: %.*s:%.*s\r\n", http->uri.host.len, http->uri.host.ptr, http->uri.port.len, http->uri.port.ptr);
  } else {
    header (client, buf, "Host: %.*s\r\n", http->uri.host.len, http->uri.host.ptr);
  }
  header (client, buf, "Connection: close\r\n");
  header (client, buf, "\r\n");
  hin_request_write (buf);
}


