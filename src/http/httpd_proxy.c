
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

static int http_client_responded (hin_buffer_t * buffer, int ret) {
  printf ("done\n");
  return 1;
}

static int http_client_read_callback (hin_buffer_t * buffer, int ret) {
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

  hin_client_t * parent = (hin_client_t*)client->parent;
  buffer->fd = parent->sockfd;
  buffer->callback = http_client_responded;
  hin_request_write (buffer);

  printf ("received data %d '%.*s'\n", ret, (int)data.len, data.ptr);
  return 0;
}

static int http_client_sent_callback (hin_buffer_t * buffer, int ret) {
  printf ("sent done\n");
  buffer->count = buffer->sz;
  buffer->callback = http_client_read_callback;
  hin_request_read (buffer);
  return 0;
}

int http_client_start_request1 (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client->extra;
  if (master.debug & DEBUG_PROTO) printf ("proxy request begin on socket %d\n", ret);

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
  printf ("proxy request is '%.*s'\n", buf->count, buf->buffer);
  hin_request_write (buf);
}

static int connected (hin_client_t * client, int ret) {
  http_client_start_request1 (client, ret);
  return 0;
}

int hin_proxy (hin_client_t * client, const char * url) {
  httpd_client_t * http = (httpd_client_t*)client->extra;
  http->state |= HIN_SERVICE;

  hin_uri_t info;
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url);
    return -1;
  }

  char * h = strndup (info.host.ptr, info.host.len);
  char * p = strndup (info.port.ptr, info.port.len);
  hin_client_t * client1 = hin_connect (h, p, sizeof (http_client_t), &connected);
  free (h);
  free (p);

  http_client_t * http1 = (http_client_t*)client1->extra;
  http1->uri = info;
  client1->parent = client;
}



