
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "hin.h"
#include "http.h"
#include "lua.h"
#include "conf.h"

int httpd_client_read_callback (hin_buffer_t * buffer);

int httpd_client_reread (httpd_client_t * http) {
  hin_buffer_t * buffer = http->read_buffer;

  if (http->state & HIN_REQ_ENDING) return 0;

  int len = buffer->ptr - buffer->data;
  int num = 0;
  if (len > 0) {
    num = httpd_client_read_callback (buffer);
  }
  if (num > 0) {
  } else if (num == 0) {
    buffer->count = buffer->sz;
    hin_lines_request (buffer);
  } else {
    printf ("httpd %d reread error\n", http->c.sockfd);
    return -1;
  }
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
  return httpd_client_finish_request (http);
}

static int httpd_client_start_post (httpd_client_t * http, string_t * source) {
  http->post_fd = openat (AT_FDCWD, HIN_HTTPD_POST_DIRECTORY, O_RDWR | O_TMPFILE, 0600);
  if (http->post_fd < 0) { printf ("openat tmpfile failed %s\n", strerror (errno)); return -1; }
  http->state |= HIN_REQ_POST;
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
    hin_pipe_append (pipe, buf1);
  }

  hin_pipe_start (pipe);

  return consume;
}

int httpd_client_read_callback (hin_buffer_t * buffer) {
  string_t source1, * source = &source1;
  source->ptr = buffer->data;
  source->len = buffer->ptr - buffer->data;

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
    printf ("TODO chunked upload later\n");
    exit (1);
  } else if (http->post_sz > 0) {
    consume = source->len;
    if (consume > http->post_sz) consume = http->post_sz;
    httpd_client_start_post (http, source);
    used += consume;
  }

  http->headers.ptr = buffer->data;
  http->headers.len = used;

  // run lua processing
  http->status = 200;
  int hin_server_callback (hin_client_t * client);
  hin_server_callback (client);

  http->peer_flags &= ~http->disable;

  if (http->state & HIN_REQ_END) {
    printf ("httpd issued forced shutdown\n");
    return -1;
  } else if (http->peer_flags & http->disable & HIN_HTTP_CHUNKED_UPLOAD) {
    printf ("httpd 411 chunked upload disabled\n");
    httpd_respond_fatal (http, 411, NULL);
  } else if ((http->state & (HIN_REQ_DATA)) == 0) {
    printf ("httpd 500 missing request %x\n", http->state);
    httpd_respond_error (http, 500, NULL);
    return -1;
  } else if (http->state & (HIN_REQ_ERROR)) {
  } else if ((http->state & HIN_REQ_CGI) && (http->state & HIN_REQ_POST)) {
    httpd_client_handle_post (http, source);
  }

  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    int hin_client_deflate_init (httpd_client_t * http);
    hin_client_deflate_init (http);
  }

  return used;
}


