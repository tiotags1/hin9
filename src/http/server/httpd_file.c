
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef STATX_MTIME
#include <linux/stat.h>
#endif

#include <basic_pattern.h>
#include <basic_vfs.h>

#include "hin.h"
#include "http.h"
#include "file.h"
#include "conf.h"
#include "vhost.h"

static int httpd_close_filefd (httpd_client_t * http) {
  if (http->file_fd <= 0) return 0;
  if (http->debug & DEBUG_SYSCALL) printf ("close filefd %d\n", http->file_fd);
  close (http->file_fd);
  http->file_fd = 0;
  return 0;
}

extern basic_vfs_t * vfs;

static int done_file (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d file done %lld\n", pipe->in.fd, pipe->out.fd, (long long)pipe->count);
  httpd_client_t * http = pipe->parent;
  httpd_client_finish_request (http, pipe);
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
    httpd_error (http, 416, "out of range %s %ld+%ld>%ld", http->file_path, http->pos, http->count, sz);
    httpd_close_filefd (http);
    return 0;
  }

  if (http->debug & DEBUG_PIPE) {
    printf ("pipe %d>%d file '%s' sz %lld\n", item->fd, http->c.sockfd, http->file_path, (long long)sz);
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

  if (HIN_HTTPD_MAX_COMPRESS_SIZE && sz > HIN_HTTPD_MAX_COMPRESS_SIZE) {
    http->disable |= HIN_HTTP_COMPRESS;
  }
  http->peer_flags &= ~http->disable;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = item->fd;
  pipe->in.flags = HIN_OFFSETS | HIN_COUNT;
  pipe->in.pos = http->pos;
  pipe->finish_callback = done_file;
  pipe->flags |= HIN_CONDENSE;

  if (item->type) { pipe->parent1 = item; }

  httpd_pipe_set_http11_response_options (http, pipe);

  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));
  httpd_write_common_headers (http, buf);

  if ((http->disable & HIN_HTTP_MODIFIED) == 0 && item->modified) {
    header_date (buf, "Last-Modified: " HIN_HTTP_DATE_FORMAT "\r\n", item->modified);
  }
  if ((http->disable & HIN_HTTP_ETAG) == 0 && item->etag) {
    header (buf, "ETag: \"%llx\"\r\n", item->etag);
  }
  if ((http->peer_flags & HIN_HTTP_CHUNKED) == 0) {
    header (buf, "Content-Length: %lld\r\n", sz);
  }
  if ((http->disable & HIN_HTTP_RANGE) == 0 && (http->peer_flags & HIN_HTTP_CHUNKED) == 0) {
    header (buf, "Accept-Ranges: bytes\r\n");
  }
  if (http->status == 206) {
    header (buf, "Content-Range: bytes %lld-%lld/%lld\r\n", http->pos, http->pos+http->count-1, sz);
  }

  header (buf, "\r\n");

  if (http->debug & DEBUG_RW) printf ("httpd %d file response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);

  hin_pipe_append_raw (pipe, buf);
  hin_pipe_start (pipe);

  return 0;
}

static int httpd_statx_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)client;
  if (ret < 0) {
    httpd_error (http, 404, "can't open '%s': %s", http->file_path, strerror (-ret));
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
    int hin_server_error_callback (hin_client_t * client, int error_code, const char * msg);
    if (hin_server_error_callback (client, 404, "can't open") == 0)
      httpd_error (http, 404, "can't open '%s': %s", http->file_path, strerror (-ret));
    return -1;
  }
  http->file_fd = ret;
  memset (buf->ptr, 0, sizeof (struct statx));

  if (HIN_HTTPD_ASYNC_STATX) {
    buf->flags |= HIN_SYNC;
  }
  buf->callback = httpd_statx_callback;
  hin_request_statx (buf, ret, "", AT_EMPTY_PATH, STATX_ALL);
  return 0;
}

int httpd_handle_file_request (hin_client_t * client, const char * path, off_t pos, off_t count, uintptr_t param) {
  httpd_client_t * http = (httpd_client_t*)client;

  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= HIN_REQ_DATA;
  http->peer_flags &= ~(HIN_HTTP_CHUNKED);

  if (http->method == HIN_METHOD_POST) {
    httpd_error (http, 405, "post on a file resource");
    return 0;
  }

  if (http->file) {
    basic_vfs_node_t * node = http->file;
    basic_vfs_file_t * file = basic_vfs_get_file (vfs, node);
    if (file == NULL) {
      httpd_error (http, 404, "can't open");
      return 0;
    }
    if (node->flags & BASIC_VFS_FORBIDDEN) {
      httpd_error (http, 403, "can't open");
      return 0;
    }

    hin_cache_item_t item;
    memset (&item, 0, sizeof (item));
    item.type = 0;
    item.fd = file->fd;
    item.modified = file->modified;
    item.size = file->size;
    item.etag = file->etag;

    httpd_send_file (http, &item, NULL);
    return 0;
  } else if (path == NULL) {
    httpd_error (http, 404, "no file path given");
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

  hin_server_t * socket = http->c.parent;
  httpd_vhost_t * vhost = socket->c.parent;

  if (HIN_HTTPD_ASYNC_OPEN) {
    buf->flags |= HIN_SYNC;
  }
  buf->callback = httpd_open_filefd_callback;
  hin_request_openat (buf, vhost->cwd_fd, path, O_RDONLY | O_CLOEXEC, 0);
  return 0;
}


