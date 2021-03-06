
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin/hin.h"
#include "hin/conf.h"
#include "hin/http/http.h"

int httpd_write_common_headers (httpd_client_t * http, hin_buffer_t * buf) {
  if ((http->disable & HIN_HTTP_DATE) == 0) {
    time_t rawtime;
    time (&rawtime);
    header_date (buf, "Date: " HIN_HTTP_DATE_FORMAT "\r\n", rawtime);
  }
  if ((http->disable & HIN_HTTP_BANNER) == 0) {
    header (buf, "Server: %s\r\n", HIN_HTTPD_SERVER_BANNER);
  }
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    header (buf, "Content-Encoding: deflate\r\n");
  } else if (http->peer_flags & HIN_HTTP_GZIP) {
    header (buf, "Content-Encoding: gzip\r\n");
  }
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (buf, "Transfer-Encoding: chunked\r\n");
  }
  if ((http->disable & HIN_HTTP_CACHE) == 0 && http->cache_flags) {
  //  header_cache_control (buf, http->cache_flags, http->cache);
  }
  if (http->peer_flags & HIN_HTTP_KEEPALIVE) {
    header (buf, "Connection: keep-alive\r\n");
  } else {
    header (buf, "Connection: close\r\n");
  }
  if (http->content_type) {
    header (buf, "Content-Type: %s\r\n", http->content_type);
  }
  if (http->append_headers) {
    header (buf, "%s", http->append_headers);
  }
  return 0;
}

static int http_raw_response_callback (hin_buffer_t * buffer, int ret) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;

  if (ret < 0) {
    printf ("httpd sending error %s\n", strerror (-ret));
  } else if (hin_buffer_continue_write (buffer, ret) > 0) {
    http->count += ret;
    return 0;
  }

  http->count += ret;

  http->state &= ~HIN_REQ_ERROR;
  httpd_client_shutdown (http);

  return 1;
}

int httpd_respond_text (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= HIN_REQ_DATA;

  http->status = status;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = http_raw_response_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  int freeable = 0;
  if (body == NULL) {
    freeable = 1;
    if (asprintf ((char**)&body, "<html><head></head><body><h1>Error %d: %s</h1></body></html>\n", status, http_status_name (status)) < 0)
      perror ("asprintf");
  }
  http->disable |= HIN_HTTP_CHUNKED | HIN_HTTP_COMPRESS | HIN_HTTP_CACHE;
  http->peer_flags &= ~ http->disable;
  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));

  httpd_write_common_headers (http, buf);
  header (buf, "Content-Length: %ld\r\n", strlen (body));
  header (buf, "\r\n");
  if (http->method != HIN_METHOD_HEAD) {
    header (buf, "%s", body);
  }
  if (freeable) free ((char*)body);
  if (http->debug & DEBUG_RW) printf ("httpd %d raw response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);
  hin_request_write (buf);
  return 0;
}

int httpd_client_read_callback (hin_buffer_t * buffer, int received) {
  string_t source, line;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  source.ptr = lines->base;
  source.len = lines->count;

  httpd_client_t * http = (httpd_client_t*)buffer->parent;

  while (1) {
    if (source.len <= 0) return 0;
    if (hin_find_line (&source, &line) == 0) return 0;
    if (line.len == 0) break;
    //printf (" %d '%.*s'\n", (int)line.len, (int)line.len, line.ptr);
  }

  httpd_respond_text (http, 200, "Hello world\n");

  return (uintptr_t)source.ptr - (uintptr_t)lines->base;
}
