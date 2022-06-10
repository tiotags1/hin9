
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include "fcgi.h"

static void hin_fcgi_stdin_done (httpd_client_t * http, hin_fcgi_worker_t * worker) {
  hin_fcgi_socket_t * socket = worker->socket;
  socket->flags &= ~HIN_FCGI_BUSY;

  if (http->debug & (DEBUG_CGI|DEBUG_POST))
    printf ("httpd %d fcgi %d worker %d stdin done\n", http->c.sockfd, worker->socket->fd, worker->req_id);

  int hin_fcgi_socket_continue (hin_fcgi_socket_t * socket);
  hin_fcgi_socket_continue (socket);
}

static int hin_fcgi_post_done_callback (hin_pipe_t * pipe) {
  httpd_client_t * http = (httpd_client_t*)pipe->parent;

  if (pipe->debug & (DEBUG_POST|DEBUG_HTTP))
    printf ("httpd %d fcgi %d post done\n", pipe->in.fd, pipe->out.fd);

  hin_fcgi_worker_t * worker = pipe->parent1;

  hin_fcgi_stdin_done (http, worker);

  worker->io_state &= ~HIN_REQ_POST;
  hin_fcgi_worker_reset (worker);

  http->state &= ~HIN_REQ_POST;
  if (http->state & HIN_REQ_DATA) return 0;
  return httpd_client_finish_request (http, pipe);
}

int hin_fcgi_post_read_callback (hin_pipe_t * pipe, hin_buffer_t * buf, int num, int flush) {
  hin_fcgi_worker_t * worker = pipe->parent1;
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = pipe->parent;

  int rounded = FCGI_ROUND_TO_PAD (num);
  int sz1 = rounded + sizeof (FCGI_Header);
  if (flush) {
    sz1 += sizeof (FCGI_Header);
  }

  hin_buffer_t * buf1 = malloc (sizeof (*buf1) + sz1);
  memset (buf1, 0, sizeof (*buf1));
  buf1->flags = socket->cflags;
  buf1->fd = socket->fd;
  buf1->count = 0;
  buf1->sz = sz1;
  buf1->ptr = buf1->buffer;
  buf1->parent = pipe;
  buf1->debug = http->debug;

  if (num > 0) {
    FCGI_Header * h = hin_fcgi_header (buf1, FCGI_STDIN, worker->req_id, num);
    char * ptr = header_ptr (buf1, rounded);
    h->padding = rounded - num;
    memcpy (ptr, buf->ptr, num);
  }

  if (flush) {
    hin_fcgi_header (buf1, FCGI_STDIN, worker->req_id, 0);
  }

  if (num == 0 && flush == 0) {
    printf ("error! fcgi write unexpected\n");
  }
  hin_pipe_append_raw (pipe, buf1);
  return 1;
}

int hin_fcgi_write_post (hin_buffer_t * buf, hin_fcgi_worker_t * worker) {
  httpd_client_t * http = worker->http;
  hin_fcgi_socket_t * socket = worker->socket;

  if (http->method != HIN_METHOD_POST) {
    hin_fcgi_header (buf, FCGI_STDIN, worker->req_id, 0);
    hin_fcgi_stdin_done (http, worker);
    return 0;
  }

  if (http->debug & (DEBUG_POST|DEBUG_HTTP))
    printf ("httpd %d fcgi %d start post\n", http->c.sockfd, socket->fd);

  string_t source, line;
  source = http->headers;
  while (1) {
    if (hin_find_line (&source, &line) == 0) { return 0; }
    if (line.len == 0) break;
  }
  if (source.len > (uintptr_t)http->post_sz) {
    source.len = http->post_sz;
  }

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = http->c.sockfd;
  pipe->in.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->in.ssl = &http->c.ssl;
  pipe->in.pos = 0;
  pipe->out.fd = buf->fd;
  pipe->out.flags = socket->cflags;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->parent1 = worker;
  pipe->read_callback = hin_fcgi_post_read_callback;
  pipe->finish_callback = hin_fcgi_post_done_callback;
  pipe->debug = http->debug;

  pipe->in.flags |= HIN_COUNT;
  pipe->left = pipe->sz = http->post_sz - source.len;

  if (source.len) {
    int num = source.len;
    int rounded = FCGI_ROUND_TO_PAD (num);
    FCGI_Header * h = hin_fcgi_header (buf, FCGI_STDIN, worker->req_id, num);
    char * ptr = header_ptr (buf, rounded);
    h->padding = rounded - num;
    memcpy (ptr, source.ptr, num);
  }

  worker->io_state |= HIN_REQ_POST;
  http->state |= HIN_REQ_POST;

  hin_pipe_start (pipe);
  return 0;
}



