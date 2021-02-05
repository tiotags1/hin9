
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
#include "conf.h"

int httpd_write_common_headers (hin_client_t * client, hin_buffer_t * buf) {
  httpd_client_t * http = (httpd_client_t *)client;

  if ((http->disable & HIN_HTTP_DATE) == 0) {
    time_t rawtime;
    time (&rawtime);
    header_date (client, buf, "Date", rawtime);
  }
  if ((http->disable & HIN_HTTP_SERVNAME) == 0) {
    header (client, buf, "Server: %s\r\n", HIN_HTTPD_SERVER_NAME);
  }
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    header (client, buf, "Content-Encoding: deflate\r\n");
  }
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (client, buf, "Transfer-Encoding: chunked\r\n");
  }
  if ((http->disable & HIN_HTTP_CACHE) == 0) {
    if (http->cache > 0) {
      header (client, buf, "Cache-Control: public, max-age=%ld\r\n", http->cache);
    } else if (http->cache < 0) {
      header (client, buf, "Cache-Control: no-cache, no-store\r\n");
    }
  }
  if (http->peer_flags & HIN_HTTP_KEEPALIVE) {
    header (client, buf, "Connection: keep-alive\r\n");
  } else {
    header (client, buf, "Connection: close\r\n");
  }
  if (http->content_type) {
    header (client, buf, "Content-Type: %s\r\n", http->content_type);
  }
  if (http->append_headers) {
    header (client, buf, "%s", http->append_headers);
  }
  return 0;
}

static int http_error_write_callback (hin_buffer_t * buffer, int ret) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;
  if (ret < 0) { printf ("httpd sending error %s\n", strerror (-ret)); }
  else if (ret != buffer->count) printf ("httpd http_error_write_callback not sent all of it %d/%d\n", ret, buffer->count);
  httpd_client_finish_request (http);
  return 1;
}

int httpd_respond_text (httpd_client_t * http, int status, const char * body) {
  hin_client_t * client = &http->c;

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
    if (asprintf ((char**)&body, "<html><head></head><body><h1>Error %d: %s</h1></body></html>\n", status, http_status_name (status)) < 0)
      perror ("asprintf");
  }
  http->disable |= HIN_HTTP_CHUNKED | HIN_HTTP_DEFLATE | HIN_HTTP_CACHE;
  http->peer_flags &= ~ http->disable;
  header (client, buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));
  httpd_write_common_headers (client, buf);
  header (client, buf, "Content-Length: %ld\r\n", strlen (body));
  header (client, buf, "\r\n");
  header (client, buf, "%s", body);
  if (freeable) free ((char*)body);
  hin_request_write (buf);

  http->state |= HIN_REQ_DATA;
}

int httpd_respond_error (httpd_client_t * http, int status, const char * body) {
  return httpd_respond_text (http, status, body);
}

int httpd_respond_fatal (httpd_client_t * http, int status, const char * body) {
  httpd_respond_text (http, status, body);
  httpd_client_shutdown (http);
}

static int httpd_close_filefd_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) printf ("encountered close in fd %d: %s\n", buffer->fd, strerror (-ret));
  return 1;
}

static int httpd_close_filefd (hin_buffer_t * buffer, httpd_client_t * http) {
  close (http->file_fd);
  return 0;
}

static int httpd_pipe_error_callback (hin_pipe_t * pipe) {
  printf ("error in client %d\n", pipe->out.fd);
  httpd_client_shutdown (pipe->parent);
}

static int done_file (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("pipe file transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  httpd_client_finish_request (pipe->parent);
}

static int httpd_statx_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)client;
  if (ret < 0) {
    printf ("http 404 error can't open '%s': %s\n", http->file_path, strerror (-ret));
    httpd_respond_error (http, 404, NULL);
    httpd_close_filefd (buf, http);
    return 0;
  }
  struct statx stat1 = *(struct statx *)buf->ptr;
  struct statx * stat = &stat1;

  off_t sz = stat->stx_size;

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
    httpd_respond_error (http, 416, NULL);
    httpd_close_filefd (buf, http);
    return 0;
  }

  if (master.debug & DEBUG_PIPE) {
    printf ("fstat %s size %ld\n", http->file_path, sz);
    printf ("sending file %s to sockfd %d filefd %d\n", http->file_path, client->sockfd, http->file_fd);
  }

  uint64_t etag = 0;
  etag += stat->stx_mtime.tv_sec * 0xffff;
  etag += stat->stx_mtime.tv_nsec * 0xff;
  etag += stat->stx_size;

  time_t rawtime;

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

  if (http->disable & HIN_HTTP_CHUNKED) {
    http->peer_flags &= ~HIN_HTTP_KEEPALIVE;
  }
  if (HIN_HTTPD_MAX_DEFLATE_SIZE && sz > HIN_HTTPD_MAX_DEFLATE_SIZE) {
    http->disable |= HIN_HTTP_DEFLATE;
  }
  http->peer_flags &= ~http->disable;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = http->file_fd;
  pipe->in.flags = HIN_OFFSETS;
  pipe->in.pos = http->pos;
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->out.ssl = &client->ssl;
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->finish_callback = done_file;
  pipe->out_error_callback = httpd_pipe_error_callback;

  int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe);
  httpd_pipe_set_chunked (http, pipe);

  if ((http->peer_flags & HIN_HTTP_CHUNKED) == 0 && http->count > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->count = pipe->sz = http->count;
  }

  buf->parent = pipe;

  if (http->status == 304) {
    pipe->count = 0;
    pipe->in.flags |= HIN_COUNT;
  }

  header (client, buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));
  httpd_write_common_headers (client, buf);

  if ((http->disable & HIN_HTTP_MODIFIED) == 0) {
    header_date (client, buf, "Last-Modified", stat->stx_mtime.tv_sec);
  }
  if ((http->disable & HIN_HTTP_ETAG) == 0) {
    header (client, buf, "ETag: \"%lx\"\r\n", etag);
  }
  if (sz && (http->peer_flags & HIN_HTTP_CHUNKED) == 0) {
    header (client, buf, "Content-Length: %ld\r\n", sz);
  }
  if ((http->disable & HIN_HTTP_RANGE) == 0) {
    header (client, buf, "Accept-Ranges: bytes\r\n");
  }
  if (http->status == 206) {
    header (client, buf, "Content-Range: bytes %ld-%ld/%ld\r\n", http->pos, http->pos+http->count-1, sz);
  }

  header (client, buf, "\r\n");

  if (master.debug & DEBUG_RW) printf ("responding '\n%.*s'\n", buf->count, buf->ptr);

  hin_pipe_write (pipe, buf);
  hin_pipe_start (pipe);

  return 0;
}

static int httpd_open_filefd_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)client;
  if (ret < 0) {
    printf ("http 404 error can't open '%s': %s\n", http->file_path, strerror (-ret));
    httpd_respond_error (http, 404, NULL);
    return -1;
  }
  http->file_fd = ret;
  memset (buf->ptr, 0, sizeof (struct statx));

  if (HIN_HTTPD_ASYNC_STATX) {
    buf->callback = httpd_statx_callback;
    hin_request_statx (buf, ret, "", AT_EMPTY_PATH, STATX_ALL);
    return 0;
  } else {
    int ret1 = statx (ret, "", AT_EMPTY_PATH, STATX_ALL, (struct statx *)buf->ptr);
    if (ret1 < 0) ret1 = -errno;
    return httpd_statx_callback (buf, ret1);
  }
}

int httpd_handle_file_request (hin_client_t * client, const char * path, off_t pos, off_t count, uintptr_t param) {
  httpd_client_t * http = (httpd_client_t*)client;

  if (http->method != HIN_HTTP_GET) {
    printf ("httpd 405 post on a file resource\n");
    httpd_respond_fatal (http, 405, NULL);
    return 0;
  }

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

  if (HIN_HTTPD_ASYNC_OPEN) {
    buf->callback = httpd_open_filefd_callback;
    hin_request_openat (buf, AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
    return 0;
  } else {
    int ret1 = openat (AT_FDCWD, path, O_RDONLY | O_CLOEXEC, 0);
    if (ret1 < 0) ret1 = -errno;
    return httpd_open_filefd_callback (buf, ret1);
  }
}


