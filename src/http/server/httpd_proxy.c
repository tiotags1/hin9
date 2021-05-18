
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"
#include "file.h"

int http_client_finish_request (http_client_t * http);
int hin_cache_finish (httpd_client_t * client, hin_pipe_t * pipe);

static int httpd_proxy_close (http_client_t * http) {
  httpd_client_t * parent = (httpd_client_t*)http->c.parent;

  if (parent) {
    if (http->debug & DEBUG_PROXY) printf ("proxy close %d>%d\n", http->c.sockfd, parent->c.sockfd);
    parent->state &= ~HIN_REQ_PROXY;
    if ((parent->state & HIN_REQ_ERROR) == 0) {
      parent->state &= ~(HIN_REQ_PROXY | HIN_REQ_DATA);
      httpd_client_finish_request (parent);
    }
  } else {
    if (http->debug & DEBUG_PROXY) printf ("proxy close %d\n", http->c.sockfd);
  }

  if (http->c.sockfd >= 0) {
    http_client_finish_request (http);
  }
  return 0;
}

static int httpd_proxy_pipe_close (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_PROXY) printf ("pipe %d>%d proxy pipe close\n", pipe->in.fd, pipe->out.fd);
  http_client_t * http = pipe->parent1;
  http->io_state &= ~HIN_REQ_DATA;

  httpd_client_t * parent = pipe->parent;
  if (parent->c.type == HIN_CACHE_OBJECT) {
    if (http->c.sockfd >= 0) {
      http_client_finish_request (http);
    }
    hin_cache_finish (parent, pipe);
    return 0;
  }

  if (http->io_state & HIN_REQ_POST) return 0;
  return httpd_proxy_close (http);
}

static int httpd_proxy_pipe_post_close (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_PROXY) printf ("pipe %d>%d proxy close post data\n", pipe->in.fd, pipe->out.fd);
  http_client_t * http = pipe->parent1;
  http->io_state &= ~HIN_REQ_POST;
  if (http->io_state & HIN_REQ_DATA) return 0;
  return httpd_proxy_close (http);
}

static int httpd_proxy_pipe_in_error (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_PROXY) printf ("pipe %d>%d proxy server connection error\n", pipe->in.fd, pipe->out.fd);
  int http_client_shutdown (http_client_t * http);
  return http_client_shutdown (pipe->parent1);
}

static int httpd_proxy_pipe_out_error (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_PROXY) printf ("pipe %d>%d proxy requester error\n", pipe->in.fd, pipe->out.fd);
  return 0;
}

static int httpd_proxy_buffer_close (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;
  if (http->debug & DEBUG_PROXY) printf ("proxy close %d buffer\n", buffer->fd);
  if (ret < 0) printf ("proxy close %d error %s\n", buffer->fd, strerror (-ret));
  if (http->c.parent)
    httpd_proxy_close (http);
  return http_client_shutdown (http);
}

static int httpd_proxy_headers_read_callback (hin_buffer_t * buffer) {
  hin_client_t * client1 = (hin_client_t*)buffer->parent;
  http_client_t * http1 = (http_client_t*)client1;
  hin_client_t * client = (hin_client_t*)client1->parent;
  httpd_client_t * http = (httpd_client_t*)client;

  string_t source, orig, line, param1, param2;
  source.ptr = buffer->data;
  source.len = buffer->ptr - buffer->data;
  orig = source;

  if (source.len > HIN_HTTPD_MAX_HEADER_SIZE) return -1;
  if (client == NULL) return 0;

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
  }
  source = orig;

  off_t sz = 0;
  uint32_t flags = 0;
  if (find_line (&source, &line) == 0 || match_string (&line, "HTTP/1.([01]) (%d+) %w+", &param1, &param2) <= 0) {
    printf ("proxy: error parsing header line '%.*s'\n", (int)line.len, line.ptr);
    httpd_respond_fatal (http, 502, NULL);
    return 0;
  }

  if (*param1.ptr == '0') flags |= HIN_HTTP_VER0;
  http->status = atoi (param2.ptr);
  if (http->debug & DEBUG_PROXY) printf ("httpd %d proxy headers %d status %d\n", http->c.sockfd, http1->c.sockfd, http->status);

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
    if (http->debug & DEBUG_PROXY)
      printf (" %ld '%.*s'\n", line.len, (int)line.len, line.ptr);
    if (matchi_string_equal (&line, "Content-Length: (%d+)", &param1) > 0) {
      sz = atoi (param1.ptr);
      if (http->debug & DEBUG_PROXY) printf ("  size is %ld\n", sz);
    } else if (matchi_string_equal (&line, "Transfer-Encoding: (%w+)", &param1) > 0) {
      if (matchi_string_equal (&param1, "chunked") > 0) {
        http1->flags |= HIN_HTTP_CHUNKED;
      } else {
        printf ("proxy: encoding type not supported '%.*s'\n", (int)param1.len, param1.ptr);
        httpd_respond_fatal (http, 502, NULL);
        return 0;
      }
    } else if (match_string (&line, "Cache-Control:") > 0) {
      httpd_parse_cache_str (line.ptr, line.len, &http->cache_flags, &http->cache);
    }
  }

  int len = source.len;
  if (sz && sz < len) len = sz;

  http1->io_state &= ~HIN_REQ_DATA;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = client1->sockfd;
  pipe->in.flags = HIN_SOCKET | (client1->flags & HIN_SSL);
  pipe->in.ssl = &client1->ssl;
  pipe->in.pos = 0;
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->out.ssl = &client->ssl;
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->parent1 = client1;
  pipe->finish_callback = httpd_proxy_pipe_close;
  pipe->in_error_callback = httpd_proxy_pipe_in_error;
  pipe->out_error_callback = httpd_proxy_pipe_out_error;
  pipe->debug = http->debug;

  if (http1->flags & HIN_HTTP_CHUNKED) {
    int hin_pipe_decode_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
    pipe->decode_callback = hin_pipe_decode_chunked;
    http->peer_flags |= HIN_HTTP_CHUNKED;
  } else if (sz > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = sz;
  }

  if ((http->cache_flags & HIN_CACHE_PUBLIC) && http->method != HIN_HTTP_POST && http->status == 200) {
    // cache check is somewhere else
    int hin_cache_save (void * store, hin_pipe_t * pipe);
    int n = hin_cache_save (NULL, pipe);
    if (n == 0) {
      // pipe already started ?
      free (pipe);
      return 0;
    } else if (n > 0) {
      if (len > 0) {
        hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source.ptr, len);
        buf1->fd = pipe->out.fd;
        buf1->flags = pipe->out.flags;
        buf1->ssl = pipe->out.ssl;
        hin_pipe_append (pipe, buf1);
      }
      hin_pipe_start (pipe);
      return (uintptr_t)source.ptr - (uintptr_t)orig.ptr;
    }
  }

  int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe);
  httpd_pipe_set_chunked (http, pipe);

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->fd = http->c.sockfd;
  buf->flags = http->c.flags;
  buf->ssl = &http->c.ssl;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = pipe;
  buf->debug = http->debug;

  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));
  httpd_write_common_headers (http, buf);
  if (sz && (http->peer_flags & HIN_HTTP_CHUNKED) == 0)
    header (buf, "Content-Length: %ld\r\n", sz);
  header (buf, "\r\n");

  if (http->debug & DEBUG_RW) printf ("httpd %d proxy response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);

  hin_pipe_write (pipe, buf);

  if (len > 0) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source.ptr, len);
    buf1->fd = buf->fd;
    buf1->flags = buf->flags;
    buf1->ssl = buf->ssl;
    hin_pipe_append (pipe, buf1);
  }

  hin_pipe_start (pipe);

  source.ptr += len;
  source.len -= len;

  return (uintptr_t)source.ptr - (uintptr_t)orig.ptr;
}

static int http_client_sent_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) {
    printf ("sent callback error %s\n", strerror (-ret));
    httpd_proxy_close (buffer->parent);
    return -1;
  }

  http_client_t * proxy = buffer->parent;
  httpd_client_t * http = proxy->c.parent;

  if (http->method != HIN_HTTP_POST) return 1;

  string_t source = http->headers, line;
  if (find_line (&source, &line) == 0) { return -1; }

  while (1) {
    if (find_line (&source, &line) == 0) { return -1; }
    if (line.len == 0) break;
  }

  if (http->debug & (DEBUG_PROXY|DEBUG_POST)) printf ("proxy post %d>%d sz is %ld\n", http->c.sockfd, proxy->c.sockfd, http->post_sz);

  off_t sz = http->post_sz;
  int len = source.len;
  if (sz && sz < len) len = sz;

  proxy->io_state &= ~HIN_REQ_POST;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
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
  pipe->finish_callback = httpd_proxy_pipe_post_close;
  //pipe->out_error_callback = httpd_proxy_pipe_in_error;
  //pipe->in_error_callback = httpd_proxy_pipe_out_error;
  pipe->debug = http->debug;

  if (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) {
    int httpd_pipe_upload_chunked (httpd_client_t * http, hin_pipe_t * pipe);
    httpd_pipe_upload_chunked (http, pipe);
  } else if (sz > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = sz;
  }
  hin_pipe_start (pipe);
  return 1;
}

int http_proxy_start_request (http_client_t * http, int ret) {
  hin_client_t * client = &http->c;
  httpd_client_t * parent = (httpd_client_t*)http->c.parent;

  if (ret < 0) {
    printf ("proxy connection failed '%s'\n", strerror (-ret));
    httpd_respond_error (parent, 502, NULL);
    httpd_proxy_close (http);
    return -1;
  }

  if (http->debug & (DEBUG_HTTP|DEBUG_PROXY)) printf ("httpd %d proxy request begin on socket %d\n", parent->c.sockfd, ret);

  hin_lines_t * lines = (hin_lines_t*)&http->read_buffer->buffer;
  lines->read_callback = httpd_proxy_headers_read_callback;
  lines->close_callback = httpd_proxy_buffer_close;

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
  buf->debug = http->debug;

  char * path = http->uri.path.ptr;
  char * path_max = path + http->uri.path.len;
  if (http->uri.query.len > 0) {
    path_max = http->uri.query.ptr + http->uri.query.len;
  }

  string_t source = parent->headers, line;
  if (find_line (&source, &line) == 0) { return -1; }

  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
  }

  const char * method = "GET";
  switch (parent->method) {
  case HIN_HTTP_GET: method = "GET"; break;
  case HIN_HTTP_POST: method = "POST"; break;
  case HIN_HTTP_HEAD: method = "HEAD"; break;
  default: printf ("bad method detected\n"); break;
  }

  header (buf, "%s %.*s HTTP/1.1\r\n", method, path_max - path, path);
  if (http->uri.port.len > 0) {
    header (buf, "Host: %.*s:%.*s\r\n", http->uri.host.len, http->uri.host.ptr, http->uri.port.len, http->uri.port.ptr);
  } else {
    header (buf, "Host: %.*s\r\n", http->uri.host.len, http->uri.host.ptr);
  }
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    header (buf, "Connection: keep-alive\r\n");
  } else {
    header (buf, "Connection: close\r\n");
  }
  if (parent->method == HIN_HTTP_POST) {
    if (parent->post_sz > 0)
      header (buf, "Content-Length: %ld\r\n", parent->post_sz);
    if (parent->post_sep)
      header (buf, "Content-Type: multipart/form-data; boundary=%s\r\n", parent->post_sep+2);
  }
  header (buf, "\r\n");
  if (parent->post_sz > 0) {
    int len = source.len;
    if (len > parent->post_sz) len = parent->post_sz;
    header_raw (buf, source.ptr, len);
  }
  if (http->debug & DEBUG_RW) printf ("httpd %d proxy request %d '\n%.*s'\n", parent->c.sockfd, buf->count, buf->count, buf->ptr);
  hin_request_write (buf);
  return 0;
}

http_client_t * hin_http_connect (http_client_t * http1, string_t * host, string_t * port, int (*finish_callback) (http_client_t * http, int ret));

http_client_t * hin_proxy (hin_client_t * parent_c, const char * url1) {
  httpd_client_t * parent = (httpd_client_t*)parent_c;

  if (parent->state & HIN_REQ_DATA) return NULL;
  parent->state |= HIN_REQ_DATA | HIN_REQ_PROXY;

  int hin_cache_check (void * store, httpd_client_t * client);
  if (hin_cache_check (NULL, parent) > 0) {
    return 0;
  }

  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url);
    free (url);
    return NULL;
  }

  http_client_t * http = calloc (1, sizeof (*http));
  http->c.parent = parent;
  http->uri = info;
  http->debug = parent->debug;

  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }

  http = hin_http_connect (http, &info.host, &info.port, http_proxy_start_request);
  return http;
}



