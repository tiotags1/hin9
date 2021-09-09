
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

static int hin_http_pipe_finish_callback (hin_pipe_t * pipe) {
  http_client_t * http = (http_client_t*)pipe->parent;
  if (pipe->debug & DEBUG_PIPE) printf ("http %d download file transfer finished infd %d outfd %d\n", http->c.sockfd, pipe->in.fd, pipe->out.fd);

  hin_http_state (http, HIN_HTTP_STATE_FINISH);

  if (http->save_fd) {
    close (http->save_fd);
    http->save_fd = 0;
  }

  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    //hin_client_list_add (&master.connection_list, &http->c);
  } else {
    http_client_shutdown (http);
  }

  int http_client_finish_request (http_client_t * http);
  http_client_finish_request (http);
  return 0;
}

int http_client_start_pipe (http_client_t * http, string_t * source) {
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
  pipe->out.fd = 1;
  pipe->out.flags = 0;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->finish_callback = hin_http_pipe_finish_callback;
  if (http->read_callback)
    pipe->read_callback = http->read_callback;
  pipe->debug = http->debug;

  if (http->save_fd) {
    pipe->out.fd = http->save_fd;
    pipe->out.flags |= HIN_FILE | HIN_OFFSETS;
  }

  if (http->flags & HIN_HTTP_CHUNKED) {
    int hin_pipe_decode_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
    pipe->decode_callback = hin_pipe_decode_chunked;
  } else if (sz > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = sz;
  }

  if (len > 0) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
    hin_pipe_append (pipe, buf1);
  }

  hin_pipe_start (pipe);

  source->ptr += len;
  source->len -= len;

  return len;
}



