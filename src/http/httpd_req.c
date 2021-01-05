
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <hin.h>
#include <basic_pattern.h>
#include "http.h"

static int http_error_write_callback (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  if (ret != buffer->count) printf ("error http_error_write_callback not sent all of it %d/%d\n", ret, buffer->count);
  httpd_client_finish_request (client);
  return 1;
}

int httpd_respond_error (hin_client_t * client, int status, const char * body) {
  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = http_error_write_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;

  int freeable = 0;
  if (body == NULL) {
    freeable = 1;
    asprintf ((char**)&body, "<html><head></head><body><h1>Error %d: %s</h1></body></html>", status, http_status_name (status));
  }
  header (client, buf, "HTTP/1.1 %d %s\r\n", status, http_status_name (status));
  header (client, buf, "Content-Length: %ld\r\n", strlen (body));
  header (client, buf, "\r\n");
  header (client, buf, "%s", body);
  if (freeable) free ((char*)body);
  hin_request_write (buf);

  httpd_client_t * http = (httpd_client_t*)&client->extra;
  http->state |= HIN_REQ_DATA;
}

static int httpd_close_filefd_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) printf ("encountered close in fd %d: %s\n", buffer->fd, strerror (-ret));
  return 1;
}

static int httpd_close_filefd (hin_buffer_t * buffer, httpd_client_t * http) {
  buffer->fd = http->filefd;
  buffer->callback = httpd_close_filefd_callback;
  hin_request_close (buffer);
  return 0;
}

static int httpd_pipe_error_callback (hin_pipe_t * pipe) {
  printf ("error in client %d\n", pipe->out.fd);
  httpd_client_shutdown (pipe->parent);
}

static int httpd_headers_write_callback (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  if (ret < 0) {
    httpd_client_shutdown (client);
    return -1;
  }
  if (ret != buffer->count) printf ("httpd error not sent headers ? %d/%d\n", ret, buffer->count);
  if (http->status == 304) {
    httpd_client_finish_request (client);
    httpd_close_filefd (buffer, http);
    return 0;
  } else {
    hin_pipe_t * pipe = send_file (client, http->filefd, http->pos, http->count, 0, NULL);
    pipe->out_error_callback = httpd_pipe_error_callback;
  }
  return 1;
}

static int httpd_statx_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  if (ret < 0) {
    printf ("http 404 error can't open '%s': %s\n", http->file_path, strerror (-ret));
    httpd_respond_error (client, 404, NULL);

    httpd_close_filefd (buf, http);
    return 0;
  }
  struct statx stat1 = *(struct statx *)buf->ptr;
  struct statx * stat = &stat1;

  off_t sz = stat->stx_size;
  buf->callback = httpd_headers_write_callback;

  if (http->count < 0) http->count = sz - http->pos;
  if (http->pos != 0 || http->count != sz) {
    if (http->status != 200 || (http->disable & HIN_HTTP_RANGE)) {
      http->pos = 0;
      http->count = sz;
    } else {
      http->status = 206;
    }
  }

  if (http->pos < 0 || http->pos > sz || http->pos + http->count > sz) {
    printf ("http 416 error out of range\n");
    httpd_respond_error (client, 416, NULL);
    httpd_close_filefd (buf, http);
    return 0;
  }

  if (master.debug & DEBUG_PIPE) {
    printf ("fstat %s size %ld\n", http->file_path, sz);
    printf ("sending file %s to sockfd %d filefd %d\n", http->file_path, client->sockfd, http->filefd);
  }

  uint64_t etag = 0;
  etag += stat->stx_mtime.tv_sec * 0xffff;
  etag += stat->stx_mtime.tv_nsec * 0xff;
  etag += stat->stx_size;

  time_t rawtime;
  struct tm *info;
  char buffer[80];

  if (1) {
    if (http->disable & HIN_HTTP_MODIFIED) {
      http->modified_since = 0;
    }
    if (http->disable & HIN_HTTP_ETAG) {
      http->etag = 0;
    }
    time_t rawtime = stat->stx_mtime.tv_sec;
    if (http->modified_since && http->modified_since < rawtime) {
      http->status = 304;
    }
    if (http->etag && http->etag == etag) {
      http->status = 304;
    } else if (http->etag) {
      http->status = 200;
    }
  }

  if (http->disable & HIN_HTTP_DEFLATE) {
    http->peer_flags &= ~HIN_HTTP_DEFLATE;
  }
  if (http->disable & HIN_HTTP_CHUNKED) {
    if (http->peer_flags & HIN_HTTP_CHUNKED) {
      http->peer_flags &= ~HIN_HTTP_KEEPALIVE;
    }
    http->peer_flags &= ~HIN_HTTP_CHUNKED;
  }

  header (client, buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));
  if ((http->disable & HIN_HTTP_DATE) == 0) {
    time (&rawtime);
    info = gmtime (&rawtime);
    strftime (buffer, sizeof buffer, "%a, %d %b %Y %X GMT", info);

    header (client, buf, "Date: %s\r\n", buffer);
  }
  if ((http->disable & HIN_HTTP_MODIFIED) == 0) {
    rawtime = stat->stx_mtime.tv_sec;
    info = gmtime (&rawtime);
    strftime (buffer, sizeof buffer, "%a, %d %b %Y %X GMT", info);
    header (client, buf, "Last-Modified: %s\r\n", buffer);
  }
  if ((http->disable & HIN_HTTP_ETAG) == 0) {
    header (client, buf, "ETag: \"%lx\"\r\n", etag);
  }
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    header (client, buf, "Content-Encoding: deflate\r\n");
  }
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (client, buf, "Transfer-Encoding: chunked\r\n");
  }
  if (sz > 0 && !(http->peer_flags & HIN_HTTP_DEFLATE)) {
    header (client, buf, "Content-Length: %ld\r\n", sz);
  }
  if ((http->disable & HIN_HTTP_CACHE) == 0) {
    if (http->cache > 0) {
      header (client, buf, "Cache-Control: public, max-age=%ld\r\n", http->cache);
    } else if (http->cache < 0) {
      header (client, buf, "Cache-Control: no-cache, no-store\r\n");
    }
  }
  if ((http->disable & HIN_HTTP_RANGE) == 0) {
    header (client, buf, "Accept-Ranges: bytes\r\n");
  }
  if (http->status == 206) {
    header (client, buf, "Content-Range: bytes %ld-%ld/%ld\r\n", http->pos, http->pos+http->count-1, sz);
  }
  if (http->peer_flags & HIN_HTTP_KEEPALIVE) {
    header (client, buf, "Connection: keep-alive\r\n");
  } else {
    header (client, buf, "Connection: close\r\n");
  }
  if (http->append_headers) {
    header (client, buf, "%s", http->append_headers);
  }
  header (client, buf, "\r\n");

  if (master.debug & DEBUG_RW) printf ("responding '\n%.*s'\n", buf->count, buf->ptr);

  hin_request_write (buf);
  return 0;
}

static int httpd_open_filefd_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  if (ret < 0) {
    printf ("http 404 error can't open '%s': %s\n", http->file_path, strerror (-ret));
    httpd_respond_error (client, 404, NULL);
    return -1;
  }
  buf->callback = httpd_statx_callback;
  http->filefd = ret;
  memset (buf->ptr, 0, sizeof (struct statx));
  hin_request_statx (buf, ret, "", AT_EMPTY_PATH, STATX_ALL);
  return 0;
}

int httpd_handle_file_request (hin_client_t * client, const char * path, off_t pos, off_t count, uintptr_t param) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;

  http->file_path = strdup (path);
  http->pos = pos;
  http->count = count;
  http->state |= HIN_REQ_DATA;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;

  buf->callback = httpd_open_filefd_callback;
  hin_request_openat (buf, AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
}

int httpd_parse_req (hin_client_t * client, string_t * source) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  string_t orig = *source;

  int httpd_parse_headers (hin_client_t * client, string_t * source);
  int used = httpd_parse_headers (client, source);
  if (used <= 0) return used;

  http->headers = orig;
  if (http->headers.len > used + http->post_sz) http->headers.len = used + http->post_sz;

  if (http->disable & HIN_HTTP_KEEPALIVE) {
    http->peer_flags &= ~HIN_HTTP_KEEPALIVE;
  }
  http->state &= ~HIN_REQ_HEADERS;

  return used;
}


