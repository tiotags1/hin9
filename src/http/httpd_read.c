
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

  if (http->status & HIN_REQ_ENDING) return 0;

  int len = buffer->ptr - buffer->data;
  int num = 0;
  if (len > 0)
    num = httpd_client_read_callback (buffer);
  if (num > 0) {
    hin_buffer_eat (buffer, num);
  } else if (num == 0) {
    buffer->count = buffer->sz;
    hin_lines_request (buffer);
  } else {
    printf ("client error\n");
    httpd_client_shutdown (http);
    return -1;
  }
  return 0;
}

#include <sys/stat.h>
#include <fcntl.h>

static int post_done (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_POST)
    printf ("cgi post done %d\n", pipe->out.fd);

  httpd_client_t * http = (httpd_client_t*)pipe->parent;

  http->state &= ~HIN_REQ_POST;
  if (http->state & HIN_REQ_DATA) return 0;
  return httpd_client_finish_request (http);
}

static int httpd_client_handle_post (httpd_client_t * http, string_t * source) {
  int consume = source->len;
  if (http->post_sz && consume > http->post_sz) consume = http->post_sz;
  off_t sz = http->post_sz;
  // send post data in buffer to post handler
  http->post_fd = openat (AT_FDCWD, HIN_HTTPD_POST_DIRECTORY, O_RDWR | O_TMPFILE, 0600);
  if (http->post_fd < 0) { printf ("openat tmpfile failed %s\n", strerror (errno)); return -1; }

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

  if (http->peer_flags & HIN_HTTP_CHUNKUP) {
    int httpd_pipe_upload_chunked (httpd_client_t * http, hin_pipe_t * pipe);
    httpd_pipe_upload_chunked (http, pipe);
  } else if (sz) {
    pipe->in.flags |= HIN_COUNT;
    pipe->count = pipe->sz = sz;
  }
  if (consume) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, consume);
    hin_pipe_append (pipe, buf1);
  }

  hin_pipe_start (pipe);

  http->state |= HIN_REQ_POST;

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

  int used = httpd_parse_req (http, source);
  if (used <= 0) return used;

  if (http->post_sz > 0 && http->state & HIN_REQ_PROXY) {
    int consume = source->len > http->post_sz ? http->post_sz : source->len;
    used += consume;
  } else if (http->post_sz > 0) {
    int consume = httpd_client_handle_post (http, source);
    if (consume < 0) {  }
    used += consume;
  }

  // run lua processing
  http->status = 200;
  int hin_server_callback (hin_client_t * client);
  hin_server_callback (client);

  http->peer_flags &= ~http->disable;

  if (http->state & HIN_REQ_END) {
    printf ("httpd issued forced shutdown\n");
    return -1;
  } if ((http->state & ~(HIN_REQ_HEADERS|HIN_REQ_END)) == 0) {
    printf ("httpd 500 missing request\n");
    httpd_respond_error (http, 500, NULL);
    return -1;
  }

  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    int hin_client_deflate_init (httpd_client_t * http);
    hin_client_deflate_init (http);
  }

  return used;
}


