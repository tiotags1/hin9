
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <basic_pattern.h>

#include "hin.h"
#include "http.h"
#include "file.h"
#include "conf.h"

static hin_buffer_t * new_buffer (hin_buffer_t * buffer, int min_sz) {
  if (buffer->debug & DEBUG_RW)
    printf ("header needed to make new buffer\n");
  int sz = READ_SZ > min_sz ? READ_SZ : min_sz;
  hin_buffer_t * buf = calloc (1, sizeof (hin_buffer_t) + sz);
  buf->sz = sz;
  buf->fd = buffer->fd;
  buf->flags = buffer->flags;
  buf->callback = buffer->callback;
  buf->parent = buffer->parent;
  buf->ptr = buf->buffer;
  buf->ssl = buffer->ssl;
  buf->count = 0;
  buffer->next = buf;
  buf->prev = buffer;
  buf->debug = buffer->debug;
  return buf;
}

static int vheader (hin_buffer_t * buffer, const char * fmt, va_list ap) {
  if (buffer->next) return vheader (buffer->next, fmt, ap);
  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  va_list prev;
  va_copy (prev, ap);
  int len = vsnprintf (buffer->ptr + pos, sz, fmt, ap);
  if (len > HIN_HTTPD_MAX_HEADER_LINE_SIZE) {
    printf ("'header' failed to write more\n");
    va_end (ap);
    return 0;
  }
  if (len > sz) {
    hin_buffer_t * buf = new_buffer (buffer, len);
    return vheader (buf, fmt, prev);
  }
  buffer->count += len;
  return len;
}

int header (hin_buffer_t * buffer, const char * fmt, ...) {
  va_list ap;
  va_start (ap, fmt);

  int len = vheader (buffer, fmt, ap);

  va_end (ap);
  return len;
}

int header_raw (hin_buffer_t * buffer, const char * data, int len) {
  if (buffer->next) return header_raw (buffer->next, data, len);

  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  if (len > HIN_HTTPD_MAX_HEADER_LINE_SIZE) {
    printf ("'header_raw' failed to write more\n");
    return 0;
  }
  if (len > sz) {
    hin_buffer_t * buf = new_buffer (buffer, len);
    return header_raw (buf, data, len);
  }

  memcpy (buffer->ptr + pos, data, len);
  buffer->count += len;
  return len;
}

int header_date (hin_buffer_t * buf, const char * name, time_t time) {
  char buffer[80];
  struct tm data;
  struct tm *info = gmtime_r (&time, &data);
  if (info == NULL) { perror ("gmtime_r"); return 0; }
  strftime (buffer, sizeof buffer, "%a, %d %b %Y %X GMT", info);
  return header (buf, "%s: %s\r\n", name, buffer);
}

int httpd_write_common_headers (httpd_client_t * http, hin_buffer_t * buf) {
  if ((http->disable & HIN_HTTP_DATE) == 0) {
    time_t rawtime;
    time (&rawtime);
    header_date (buf, "Date", rawtime);
  }
  if ((http->disable & HIN_HTTP_BANNER) == 0) {
    header (buf, "Server: %s\r\n", HIN_HTTPD_SERVER_BANNER);
  }
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    header (buf, "Content-Encoding: deflate\r\n");
  }
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (buf, "Transfer-Encoding: chunked\r\n");
  }
  if ((http->disable & HIN_HTTP_CACHE) == 0 && http->cache_flags) {
    header_cache_control (buf, http->cache_flags, http->cache);
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

  if (ret < 0) { printf ("httpd sending error %s\n", strerror (-ret)); }
  else if (ret != buffer->count) printf ("httpd http_error_write_callback not sent all of it %d/%d\n", ret, buffer->count);

  http->state &= ~HIN_REQ_ERROR;
  httpd_client_finish_request (http);

  return 1;
}

int httpd_respond_text (httpd_client_t * http, int status, const char * body) {
  hin_client_t * client = &http->c;

  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= HIN_REQ_DATA;

  if (http->method == HIN_HTTP_POST) {
    printf ("httpd 405 post on a raw resource\n");
    http->method = HIN_HTTP_GET;
    httpd_respond_fatal (http, 405, NULL);
    return 0;
  }
  http->status = status;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = http_raw_response_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;
  buf->debug = http->debug;

  int freeable = 0;
  if (body == NULL) {
    freeable = 1;
    if (asprintf ((char**)&body, "<html><head></head><body><h1>Error %d: %s</h1></body></html>\n", status, http_status_name (status)) < 0)
      perror ("asprintf");
  }
  http->disable |= HIN_HTTP_CHUNKED | HIN_HTTP_DEFLATE | HIN_HTTP_CACHE;
  http->peer_flags &= ~ http->disable;
  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));
  httpd_write_common_headers (http, buf);
  header (buf, "Content-Length: %ld\r\n", strlen (body));
  header (buf, "\r\n");
  if (http->method != HIN_HTTP_HEAD) {
    header (buf, "%s", body);
  }
  if (freeable) free ((char*)body);
  if (http->debug & DEBUG_RW) printf ("httpd %d raw response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);
  hin_request_write (buf);
  return 0;
}

int httpd_respond_error (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_ERROR) return 0;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_POST);
  http->state |= HIN_REQ_ERROR;
  http->method = HIN_HTTP_GET;
  return httpd_respond_text (http, status, body);
}

int httpd_respond_fatal (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_ERROR) return 0;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_POST);
  http->state |= HIN_REQ_ERROR;
  http->method = HIN_HTTP_GET;
  httpd_respond_text (http, status, body);
  httpd_client_shutdown (http);
  return 0;
}

int httpd_respond_fatal_and_full (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_ERROR) return 0;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_POST);
  http->state |= HIN_REQ_ERROR;
  http->method = HIN_HTTP_GET;
  httpd_respond_text (http, status, body);
  httpd_client_shutdown (http);
  return 0;
}



