
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

static int hin_http_pipe_finish_callback (hin_pipe_t * pipe) {
  http_client_t * http = (http_client_t*)pipe->parent;
  if (pipe->debug & DEBUG_PIPE)
    printf ("pipe %d>%d download %d done\n", pipe->in.fd, pipe->out.fd, http->c.sockfd);

  http->io_state &= ~HIN_REQ_DATA;
  int http_client_finish_request (http_client_t * http);
  http_client_finish_request (http);

  return 0;
}

hin_pipe_t * http_client_start_pipe (http_client_t * http, string_t * source) {
  off_t len = source->len;
  off_t sz = http->sz;
  if (sz && sz < len) {
    len = sz;
  }

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = http->c.sockfd;
  pipe->in.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->in.ssl = &http->c.ssl;
  pipe->in.pos = 0;
  pipe->out.fd = STDOUT_FILENO;
  pipe->out.flags = 0;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->finish_callback = hin_http_pipe_finish_callback;
  if (http->read_callback)
    pipe->read_callback = http->read_callback;
  pipe->debug = http->debug;

  if (http->method == HIN_METHOD_HEAD) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = 0;
    len = 0;
  } else if (http->flags & HIN_HTTP_CHUNKED) {
    int hin_pipe_decode_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
    pipe->decode_callback = hin_pipe_decode_chunked;
  } else if (http->sz > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = sz;
  }

  hin_http_state (http, HIN_HTTP_STATE_HEADERS, (uintptr_t)pipe);

  if (http->save_fd) {
    pipe->out.fd = http->save_fd;
    pipe->out.flags |= HIN_FILE | HIN_OFFSETS;
  }

  if (len > 0) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
    hin_pipe_write_process (pipe, buf1, HIN_PIPE_ALL);
  }

  source->ptr += len;
  source->len -= len;

  return pipe;
}



