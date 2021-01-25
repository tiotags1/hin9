
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

static int httpd_proxy_close (http_client_t * http) {
  if (master.debug & DEBUG_PROXY) printf ("proxy close\n");
  httpd_client_t * parent = (httpd_client_t*)http->c.parent;
  parent->state &= ~HIN_REQ_PROXY;
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    hin_client_list_add (&master.connection_list, &http->c);
  } else {
    http_client_shutdown (http);
  }
  httpd_client_finish_request (parent);
}

static int httpd_proxy_pipe_close (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PROXY) printf ("proxy pipe close\n");
  http_client_t * http = pipe->parent1;
  httpd_proxy_close (http);
}

static int httpd_proxy_pipe_close1 (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PROXY) printf ("proxy pipe close post data\n");
  //http_client_t * http = pipe->parent1;
  //httpd_proxy_close (http);
}

static int httpd_proxy_pipe_in_error (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PROXY) printf ("proxy proxied server connection error\n");
  int http_client_shutdown (http_client_t * http);
  http_client_shutdown (pipe->parent1);
}

static int httpd_proxy_pipe_out_error (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PROXY) printf ("proxy requester error\n");
}

static int httpd_proxy_buffer_close (hin_buffer_t * buffer) {
  if (master.debug & DEBUG_PROXY) printf ("proxy header buffer close\n");
  http_client_t * http = (http_client_t*)buffer->parent;
  httpd_proxy_close (http);
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
    if (matchi_string_equal (&line, "Content%-Length: (%d+)", &param1) > 0) {
      sz = atoi (param1.ptr);
      if (master.debug & DEBUG_PROXY)
      printf ("proxy:   size is %ld\n", sz);
    } else if (matchi_string_equal (&line, "Transfer%-Encoding: .*") > 0) {
      printf ("proxy: requested chunked encoding but not supported yet\n");
      httpd_respond_error (http, 502, NULL);
      httpd_client_shutdown (http);
    }
  }

  int len = source.len;
  if (sz && sz < len) len = sz;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  pipe->in.fd = client1->sockfd;
  pipe->in.flags = HIN_SOCKET | (client1->flags & HIN_SSL);
  pipe->in.ssl = &client1->ssl;
  pipe->in.pos = 0;
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->in.ssl = &client->ssl;
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->parent1 = client1;
  pipe->finish_callback = httpd_proxy_pipe_close;
  pipe->in_error_callback = httpd_proxy_pipe_in_error;
  pipe->out_error_callback = httpd_proxy_pipe_out_error;
  if (sz > 0) {
    pipe->count = pipe->sz = sz - len;
    if (pipe->count == 0) pipe->flags |= HIN_DONE;
  }

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

  if (len > 0) {
    hin_buffer_t * buf1 = malloc (sizeof (*buf) + len);
    memset (buf1, 0, sizeof (*buf));
    buf1->count = buf1->sz = len;
    buf1->ptr = buf1->buffer;
    buf1->parent = pipe;
    memcpy (buf1->ptr, source.ptr, len);
    hin_pipe_append (pipe, buf1);
  }

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

  http_client_t * proxy = buffer->parent;
  httpd_client_t * http = proxy->c.parent;

  if (http->method != HIN_HTTP_POST) return 1;

  string_t source = http->headers, line, param1, param2;
  if (find_line (&source, &line) == 0) { return -1; }

  while (1) {
    if (find_line (&source, &line) == 0) { return -1; }
    if (line.len == 0) break;
  }

  if (master.debug & DEBUG_PROXY) printf ("proxy post %d>%d sz is %ld\n", http->c.sockfd, proxy->c.sockfd, http->post_sz);

  off_t sz = http->post_sz;
  int len = source.len;
  if (sz && sz < len) len = sz;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  pipe->in.fd = http->c.sockfd;
  pipe->in.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->in.ssl = &http->c.ssl;
  pipe->in.pos = 0;
  pipe->out.fd = proxy->c.sockfd;
  pipe->out.flags = HIN_DONE | HIN_SOCKET | (proxy->c.flags & HIN_SSL);
  pipe->out.ssl = &proxy->c.ssl;
  pipe->out.pos = 0;
  pipe->parent = proxy;
  pipe->parent1 = http;
  pipe->finish_callback = httpd_proxy_pipe_close1;
  //pipe->out_error_callback = httpd_proxy_pipe_in_error;
  //pipe->in_error_callback = httpd_proxy_pipe_out_error;
  if (sz > 0) {
    pipe->count = pipe->sz = sz - len;
    if (pipe->count == 0) pipe->flags |= HIN_DONE;
  }
  hin_pipe_advance (pipe);
  return 1;
}

int http_proxy_start_request (http_client_t * http, int ret) {
  hin_client_t * client = &http->c;
  httpd_client_t * parent = (httpd_client_t*)http->c.parent;
  if (master.debug & DEBUG_PROTO) printf ("proxy request begin on socket %d\n", ret);

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = http_client_sent_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = http;
  buf->ssl = &client->ssl;

  char * path = http->uri.path.ptr;
  char * path_max = path + http->uri.path.len;
  if (http->uri.query.len > 0) {
    path_max = http->uri.query.ptr + http->uri.query.len;
  }

  string_t source = parent->headers, line, param1, param2;
  if (find_line (&source, &line) == 0) { return -1; }

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
  }

  const char * method = "GET";
  if (parent->method == HIN_HTTP_POST) method = "POST";

  header (client, buf, "%s %.*s HTTP/1.0\r\n", method, path_max - path, path);
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
  if (parent->post_sz > 0) {
    header (client, buf, "Content-Length: %ld\r\n", parent->post_sz);
    header (client, buf, "Content-Type: multipart/form-data; boundary=%s\r\n", parent->post_sep+2);
  }
  header (client, buf, "\r\n");
  if (parent->post_sz) {
    int len = source.len;
    if (len > parent->post_sz) len = parent->post_sz;
    header_raw (client, buf, source.ptr, len);
  }
  if (master.debug & DEBUG_RW) printf ("proxy request is '\n%.*s'\n", buf->count, buf->ptr);
  hin_request_write (buf);
  return 0;
}

static int connected (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client;
  if (ret < 0) {
    printf ("proxy connection failed\n");
    httpd_client_t * parent = (httpd_client_t*)client->parent;
    httpd_respond_error (parent, 502, NULL);
    void http_client_clean (http_client_t * http);
    http_client_clean (http);
    return 0;
  }
  http_proxy_start_request (http, ret);
  return 0;
}

http_client_t * hin_proxy (hin_client_t * parent_c, const char * url1) {
  httpd_client_t * parent = (httpd_client_t*)parent_c;
  parent->state |= HIN_REQ_DATA | HIN_REQ_PROXY;

  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url);
    free (url);
    return NULL;
  }

  http_client_t * httpd_proxy_connection_get (string_t * host, string_t * port);
  http_client_t * http = httpd_proxy_connection_get (&info.host, &info.port);
  if (http) {
    if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
    http->c.parent = parent;
    http->uri = info;
    connected (&http->c, http->c.sockfd);
    return http;
  }

  http = calloc (1, sizeof (http_client_t));
  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }
  if (info.https) { http->c.flags |= HIN_SSL; }
  http->c.parent = parent;
  http->uri = info;

  int ret = hin_connect (&http->c, http->host, http->port, &connected);
  if (ret < 0) { printf ("can't create connection\n"); return NULL; }
  master.num_connection++;

  return http;
}



