
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

int httpd_client_read_callback (hin_buffer_t * buffer, int received);

int httpd_client_reread (httpd_client_t * http) {
  hin_buffer_t * buffer = http->read_buffer;

  if (http->state & HIN_REQ_STOPPING) return 0;

  hin_lines_reread (buffer);
  return 0;
}

#include <sys/stat.h>
#include <fcntl.h>

static int post_done (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_POST)
    printf ("cgi post done %d\n", pipe->out.fd);

  httpd_client_t * http = (httpd_client_t*)pipe->parent;

  http->state &= ~HIN_REQ_POST;
  if (http->state & HIN_REQ_DATA) return 0;
  return httpd_client_finish_request (http, pipe);
}

static int httpd_client_start_post (httpd_client_t * http, string_t * source) {
  if (http->state & HIN_REQ_CGI) {
    http->post_fd = openat (AT_FDCWD, HIN_HTTPD_POST_DIRECTORY, O_RDWR | O_TMPFILE, 0600);
    if (http->post_fd < 0) { printf ("openat tmpfile failed %s\n", strerror (errno)); return -1; }
  }
  if (http->state & HIN_REQ_FCGI) {
    return 0;
  }
  http->state |= HIN_REQ_POST;
  return 0;
}

static int httpd_client_handle_post (httpd_client_t * http, string_t * source) {
  int consume = source->len;
  off_t sz = http->post_sz;
  if (http->state & HIN_REQ_ERROR) return consume;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = http->c.sockfd;
  pipe->in.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->in.ssl = &http->c.ssl;
  pipe->in.pos = 0;
  pipe->out.fd = http->post_fd;
  pipe->out.flags = HIN_OFFSETS;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->finish_callback = post_done;
  pipe->debug = http->debug;

  if (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) {
    int httpd_pipe_upload_chunked (httpd_client_t * http, hin_pipe_t * pipe);
    httpd_pipe_upload_chunked (http, pipe);
  } else if (sz) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = sz;
  }
  if (consume) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, consume);
    hin_pipe_write_process (pipe, buf1, HIN_PIPE_ALL);
  }
  hin_pipe_start (pipe);

  return consume;
}

int httpd_client_read_callback (hin_buffer_t * buffer, int received) {
  string_t source1, * source = &source1;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  source->ptr = lines->base;
  source->len = lines->count;

  hin_client_t * client = (hin_client_t*)buffer->parent;
  httpd_client_t * http = (httpd_client_t*)client;

  if (source->len >= HIN_HTTPD_MAX_HEADER_SIZE) {
    httpd_respond_fatal (http, 413, NULL);
    return -1;
  }

  int httpd_parse_req (httpd_client_t * http, string_t * source);
  int used = httpd_parse_req (http, source);
  if (used <= 0) return used;

  int consume = 0;
  if (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) {
    httpd_error (http, 411, "chunked upload currently disabled serverwide");
    return -1;
  } else if (http->method & HIN_METHOD_POST) {
    consume = source->len;
    if (consume > http->post_sz) consume = http->post_sz;
    used += consume;
  }

  http->headers.ptr = lines->base;
  http->headers.len = used;

  // run lua processing
  http->status = 200;
  int hin_server_callback (hin_client_t * client);
  hin_server_callback (client);

  if (http->method & HIN_METHOD_POST) {
    httpd_client_start_post (http, source);
  }

  http->peer_flags &= ~http->disable;

  if (http->state & HIN_REQ_END) {
    httpd_error (http, 0, "forced shutdown");
    return -1;
  } else if (http->peer_flags & http->disable & HIN_HTTP_CHUNKED_UPLOAD) {
    httpd_error (http, 411, "chunked upload disabled");
    return -1;
  } else if ((http->state & (HIN_REQ_DATA)) == 0) {
    httpd_error (http, 500, "missing request");
    return -1;
  } else if (http->state & (HIN_REQ_ERROR)) {
  } else if ((http->state & HIN_REQ_CGI) && (http->state & HIN_REQ_POST)) {
    httpd_client_handle_post (http, source);
  }

  if (http->peer_flags & HIN_HTTP_COMPRESS) {
    int hin_client_deflate_init (httpd_client_t * http);
    hin_client_deflate_init (http);
  }

  return used;
}


