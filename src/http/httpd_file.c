
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

static int httpd_close_filefd (httpd_client_t * http) {
  if (http->file_fd <= 0) return 0;
  if (http->debug & DEBUG_SYSCALL) printf ("close filefd %d\n", http->file_fd);
  close (http->file_fd);
  http->file_fd = 0;
  return 0;
}

static int httpd_pipe_error_callback (hin_pipe_t * pipe) {
  printf ("error in client %d\n", pipe->out.fd);
  httpd_client_shutdown (pipe->parent);
  return 0;
}

static int done_file (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d file transfer finished bytes %ld\n", pipe->in.fd, pipe->out.fd, pipe->count);
  httpd_client_finish_request (pipe->parent);
  void hin_cache_unref (void *);
  if (pipe->parent1) hin_cache_unref (pipe->parent1);
  return 0;
}

int httpd_send_file (httpd_client_t * http, hin_cache_item_t * item, hin_buffer_t * buf) {
  off_t sz = item->size;

  if (buf == NULL) {
    buf = malloc (sizeof (*buf) + READ_SZ);
    memset (buf, 0, sizeof (*buf));
    buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
    buf->fd = http->c.sockfd;
    buf->count = 0;
    buf->sz = READ_SZ;
    buf->ptr = buf->buffer;
    buf->parent = http;
    buf->ssl = &http->c.ssl;
  }

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
    httpd_close_filefd (http);
    return 0;
  }

  if (http->debug & DEBUG_PIPE) {
    printf ("sending file '%s' size %ld sockfd %d filefd %d\n", http->file_path, sz, http->c.sockfd, item->fd);
  }

  // do you need to check http->status for 200 or can you return a 304 for a 206
  if ((http->disable & HIN_HTTP_MODIFIED)) {
    http->modified_since = 0;
  }
  if ((http->disable & HIN_HTTP_ETAG)) {
    http->etag = 0;
  }
  if (http->modified_since && http->modified_since < item->modified) {
    http->status = 304;
  }
  if (http->etag && http->etag == item->etag) {
    http->status = 304;
  } else if (http->etag && http->status == 304) {
    http->status = 200;
  }

  if (HIN_HTTPD_MAX_DEFLATE_SIZE && sz > HIN_HTTPD_MAX_DEFLATE_SIZE) {
    http->disable |= HIN_HTTP_DEFLATE;
  }
  http->peer_flags &= ~http->disable;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = item->fd;
  pipe->in.flags = HIN_OFFSETS;
  pipe->in.pos = http->pos;
  pipe->out.fd = http->c.sockfd;
  pipe->out.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->out.ssl = &http->c.ssl;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->finish_callback = done_file;
  pipe->out_error_callback = httpd_pipe_error_callback;
  pipe->debug = http->debug;

  buf->parent = pipe;
  if (item->type) { pipe->parent1 = item; }

  int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe);
  if (http->status == 304 || http->method == HIN_HTTP_HEAD) {
    http->peer_flags &= ~(HIN_HTTP_CHUNKED | HIN_HTTP_DEFLATE);
  } else {
    httpd_pipe_set_chunked (http, pipe);
  }

  if ((http->peer_flags & HIN_HTTP_CHUNKED) == 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = http->count;
    if (http->status == 304 || http->method == HIN_HTTP_HEAD) {
      pipe->left = 0;
    }
  }

  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));
  httpd_write_common_headers (http, buf);

  if ((http->disable & HIN_HTTP_MODIFIED) == 0 && item->modified) {
    header_date (buf, "Last-Modified", item->modified);
  }
  if ((http->disable & HIN_HTTP_ETAG) == 0 && item->etag) {
    header (buf, "ETag: \"%lx\"\r\n", item->etag);
  }
  if ((http->peer_flags & HIN_HTTP_CHUNKED) == 0) {
    header (buf, "Content-Length: %ld\r\n", sz);
  }
  if ((http->disable & HIN_HTTP_RANGE) == 0 && (http->peer_flags & HIN_HTTP_CHUNKED) == 0) {
    header (buf, "Accept-Ranges: bytes\r\n");
  }
  if (http->status == 206) {
    header (buf, "Content-Range: bytes %ld-%ld/%ld\r\n", http->pos, http->pos+http->count-1, sz);
  }

  header (buf, "\r\n");

  if (http->debug & DEBUG_RW) printf ("httpd %d file response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);

  hin_pipe_write (pipe, buf);
  hin_pipe_start (pipe);

  return 0;
}

static int httpd_statx_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)client;
  if (ret < 0) {
    printf ("http 404 error can't open1 '%s': %s\n", http->file_path, strerror (-ret));
    httpd_respond_error (http, 404, NULL);
    httpd_close_filefd (http);
    return 1;
  }
  struct statx stat1 = *(struct statx *)buf->ptr;
  struct statx * stat = &stat1;

  uint64_t etag = 0;
  etag += stat->stx_mtime.tv_sec * 0xffff;
  etag += stat->stx_mtime.tv_nsec * 0xff;
  etag += stat->stx_size;

  hin_cache_item_t item;
  memset (&item, 0, sizeof (item));
  item.fd = http->file_fd;
  item.size = stat->stx_size;
  item.etag = etag;
  item.modified = stat->stx_mtime.tv_sec;
  item.type = 0;

  httpd_send_file (http, &item, buf);

  return 0;
}

static int httpd_open_filefd_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)client;
  if (ret < 0) {
    printf ("http 404 error can't open '%s': %s\n", http->file_path, strerror (-ret));
    int hin_server_error_callback (hin_client_t * client, int error_code, const char * msg);
    if (hin_server_error_callback (client, 404, "can't open") == 0)
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

  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= HIN_REQ_DATA;
  http->peer_flags &= ~(HIN_HTTP_CHUNKED);

  if (http->method == HIN_HTTP_POST) {
    printf ("httpd 405 post on a file resource\n");
    httpd_respond_fatal (http, 405, NULL);
    return 0;
  }

  if (http->file_path) free (http->file_path);
  http->file_path = strdup (path);
  http->pos = pos;
  http->count = count;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;
  buf->debug = http->debug;

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


