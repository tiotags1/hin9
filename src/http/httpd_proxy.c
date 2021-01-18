
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

static int httpd_proxy_close (hin_client_t * client) {
  if (master.debug & DEBUG_PROXY) printf ("proxy error should close\n");
  httpd_client_t * parent = (httpd_client_t*)client->parent;
  http_client_shutdown (client);
  httpd_client_finish_request (parent);
}

static int httpd_proxy_pipe_close (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PROXY) printf ("proxy pipe close\n");
  hin_client_t * client = pipe->parent1;
  hin_client_t * parent = client->parent;
  httpd_proxy_close (client);
}

static int httpd_proxy_buffer_close (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  httpd_proxy_close (client);
}

static int httpd_proxy_headers_read_callback (hin_buffer_t * buffer) {
  hin_client_t * client1 = (hin_client_t*)buffer->parent;
  hin_client_t * client = (hin_client_t*)client1->parent;
  httpd_client_t * http = (httpd_client_t*)client;

  string_t source, orig, line, param1, param2;
  source.ptr = buffer->data;
  source.len = buffer->ptr - buffer->data;
  orig = source;

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
  }
  source = orig;

  off_t sz = 0;
  uint32_t flags = 0;
  int status = 200;
  if (find_line (&source, &line) == 0 || match_string (&line, "HTTP/1.([01]) (%d+) %w+", &param1, &param2) <= 0) {
    printf ("proxy: error parsing header line '%.*s'\n", (int)line.len, line.ptr);
    httpd_respond_error (http, 502, NULL);
    httpd_client_shutdown (http);
    return -1;
  }

  if (*param1.ptr == '0') flags |= HIN_HTTP_VER0;
  status = atoi (param2.ptr);
  if (master.debug & DEBUG_PROXY) printf ("proxy: status %d\n", status);

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
    if (master.debug & DEBUG_PROXY)
      printf ("proxy: header1 '%.*s'\n", (int)line.len, line.ptr);
    if (hin_string_equali (&line, "Content%-Length: (%d+)", &param1) > 0) {
      sz = atoi (param1.ptr);
      if (master.debug & DEBUG_PROXY)
      printf ("proxy:   size is %ld\n", sz);
    }
  }

  int len = source.len;
  if (sz && sz < len) len = sz;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  pipe->in.fd = client1->sockfd;
  pipe->in.flags = HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->in.pos = 0;
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_SOCKET;
  pipe->out.pos = 0;
  pipe->count = pipe->sz = sz > 0 ? sz - len : 0;
  pipe->parent = client;
  pipe->parent1 = client1;
  pipe->ssl = &client->ssl;
  pipe->finish_callback = httpd_proxy_pipe_close;
  if (pipe->count == 0 && sz > 0) pipe->flags |= HIN_DONE;

  httpd_client_t * http1 = (httpd_client_t*)client1;

  int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe);
  httpd_pipe_set_chunked (http, pipe);

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = pipe;

  header (client, buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));
  httpd_write_common_headers (client, buf);
  if (sz && (http->peer_flags & HIN_HTTP_CHUNKED) == 0)
    header (client, buf, "Content-Length: %ld\r\n", sz);
  header (client, buf, "\r\n");

  hin_pipe_write (pipe, buf);

  hin_buffer_t * buf1 = malloc (sizeof (*buf) + len);
  memset (buf1, 0, sizeof (*buf));
  buf1->count = buf1->sz = len;
  buf1->ptr = buf1->buffer;
  buf1->parent = pipe;
  memcpy (buf1->ptr, source.ptr, len);
  hin_pipe_append (pipe, buf1);

  hin_pipe_advance (pipe);

  source.ptr += len;
  source.len -= len;

  return (uintptr_t)source.ptr - (uintptr_t)orig.ptr;
}

static int httpd_proxy_headers_eat (hin_buffer_t * buf, int num) {
  if (num > 0) {
    hin_buffer_eat (buf, num);
  }
  if (num == 0) {
    hin_lines_request (buf);
    return 0;
  }
  return 1;
}

static int http_client_sent_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) {
    printf ("sent callback error %s\n", strerror (-ret));
    httpd_proxy_close (buffer->parent);
    return -1;
  }

  hin_buffer_t * buf = hin_lines_create_raw ();
  buf->fd = buffer->fd;
  buf->parent = (hin_client_t*)buffer->parent;
  buf->flags = buffer->flags;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->read_callback = httpd_proxy_headers_read_callback;
  lines->close_callback = httpd_proxy_buffer_close;
  lines->eat_callback = httpd_proxy_headers_eat;
  hin_request_read (buf);
  return 1;
}

int http_client_start_request1 (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client;
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
  hin_request_write (buf);
}

static int connected (hin_client_t * client, int ret) {
  if (ret < 0) {
    printf ("proxy connection failed\n");
    httpd_client_t * parent = (httpd_client_t*)client->parent;
    httpd_respond_error (parent, 502, NULL);
    return 1;
  }
  http_client_start_request1 (client, ret);
  return 0;
}

int hin_proxy (hin_client_t * client, const char * url) {
  httpd_client_t * http = (httpd_client_t*)client;
  http->state |= HIN_REQ_DATA;

  hin_uri_t info;
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url);
    return -1;
  }

  char * h = strndup (info.host.ptr, info.host.len);
  char * p = strndup (info.port.ptr, info.port.len);
  hin_client_t * client1 = calloc (1, sizeof (http_client_t));
  client1->parent = client;
  int ret = hin_connect (client1, h, p, &connected);
  if (ret < 0) { printf ("can't create connection\n"); return -1; }
  http_client_t * http1 = (http_client_t*)client1;
  http1->uri = info;
  free (h);
  free (p);
}



