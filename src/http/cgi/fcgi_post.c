
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include "fcgi.h"

static int hin_fcgi_post_done_callback (hin_pipe_t * pipe) {
  if (pipe->debug & DEBUG_POST)
    printf ("fcgi post done %d\n", pipe->out.fd);

  httpd_client_t * http = (httpd_client_t*)pipe->parent;

  http->state &= ~HIN_REQ_POST;
  if (http->state & HIN_REQ_DATA) return 0;
  return httpd_client_finish_request (http);
}

int hin_fcgi_post_read_callback (hin_pipe_t * pipe, hin_buffer_t * buf, int num, int flush) {
  hin_fcgi_worker_t * worker = pipe->parent1;
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = pipe->parent;

  int sz1 = num + 2 * sizeof (FCGI_Header);
  hin_buffer_t * buf1 = malloc (sizeof (*buf1) + sz1);
  memset (buf1, 0, sizeof (*buf1));
  buf1->fd = socket->fd;
  buf1->count = 0;
  buf1->sz = sz1;
  buf1->ptr = buf1->buffer;
  buf1->parent = pipe;
  buf1->debug = http->debug;

  if (num > 0) {
    hin_fcgi_header (buf1, FCGI_STDIN, worker->req_id, num);
    char * ptr = header_ptr (buf1, num);
    memcpy (ptr, buf->ptr, num);
  }

  if (flush) {
    hin_fcgi_header (buf1, FCGI_STDIN, worker->req_id, 0);
  }

  if (num == 0 && flush == 0) {
    printf ("error! fcgi write unexpected\n");
  }
  hin_pipe_write (pipe, buf1);
  return 1;
}

int hin_fcgi_write_post (hin_buffer_t * buf, hin_fcgi_worker_t * worker) {
  httpd_client_t * http = worker->http;

  if (http->method != HIN_HTTP_POST || http->post_sz <= 0) {
    hin_fcgi_header (buf, FCGI_STDIN, worker->req_id, 0);
    return 0;
  }

  string_t source, line;
  source = http->headers;
  while (1) {
    if (find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
  }
  if (source.len > http->post_sz) {
    source.len = http->post_sz;
  }

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = http->c.sockfd;
  pipe->in.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->in.ssl = &http->c.ssl;
  pipe->in.pos = 0;
  pipe->out.fd = buf->fd;
  pipe->out.flags = 0;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->parent1 = worker;
  pipe->read_callback = hin_fcgi_post_read_callback;
  pipe->finish_callback = hin_fcgi_post_done_callback;
  pipe->debug = http->debug;

  pipe->in.flags |= HIN_COUNT;
  pipe->left = pipe->sz = http->post_sz - source.len;

  if (source.len) {
    hin_fcgi_header (buf, FCGI_STDIN, worker->req_id, source.len);
    char * ptr = header_ptr (buf, source.len);
    memcpy (ptr, source.ptr, source.len);
  }

  hin_pipe_start (pipe);
  return 0;
}


