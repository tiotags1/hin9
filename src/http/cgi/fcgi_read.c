
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include <basic_endianness.h>

#include "fcgi.h"

static int hin_fcgi_pipe_finish_callback (hin_pipe_t * pipe) {
  hin_fcgi_worker_t * worker = pipe->parent;
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = worker->http;
  if (http && http->debug & DEBUG_CGI)
    printf ("fcgi %d worker %d done.\n", socket->fd, worker->req_id);
  hin_fcgi_worker_reset (worker);
  return 0;
}

int hin_fcgi_pipe_init (hin_fcgi_worker_t * worker) {
  httpd_client_t * http = worker->http;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = -1;
  pipe->in.flags = HIN_INACTIVE;
  pipe->in.pos = 0;
  pipe->out.fd = http->c.sockfd;
  pipe->out.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->out.ssl = &http->c.ssl;
  pipe->out.pos = 0;
  pipe->parent = worker;
  pipe->finish_callback = hin_fcgi_pipe_finish_callback;
  pipe->debug = http->debug;

  hin_buffer_t * buf = malloc (sizeof *buf + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->fd = http->c.sockfd;
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = pipe;
  buf->debug = http->debug;
  buf->ssl = &http->c.ssl;
  header (buf, "HTTP/1.1 200 OK\r\n");
  hin_pipe_write (pipe, buf);

  hin_pipe_start (pipe);

  worker->out = pipe;

  return 0;
}

static int hin_fcgi_pipe_write (hin_fcgi_worker_t * worker, FCGI_Header * head) {
  printf ("payload '%.*s'\n", head->length, head->data);
  hin_buffer_t * buf = hin_buffer_create_from_data (worker->out, (char*)head->data, head->length);
  hin_pipe_write (worker->out, buf);
  hin_pipe_advance (worker->out);
  return 0;
}

static int hin_fcgi_pipe_end (hin_fcgi_worker_t * worker, FCGI_Header * head) {
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = worker->http;
  FCGI_EndRequestBody * req = (FCGI_EndRequestBody*)head->data;
  req->appStatus = endian_swap32 (req->appStatus);
  if (http->debug & DEBUG_CGI)
    printf ("fcgi %d req_id %d status %d proto %d end\n", socket->fd, head->request_id, req->appStatus, req->protocolStatus);

  hin_pipe_t * pipe = worker->out;
  pipe->in.flags |= HIN_DONE;
  //hin_pipe_advance (pipe);
  worker->out = NULL;

  return 0;
}

int hin_fcgi_read_rec (hin_buffer_t * buf, char * ptr, int left) {
  if (left < 8) return -1;
  FCGI_Header * head = (FCGI_Header*)ptr;
  head->request_id = endian_swap16 (head->request_id);
  head->length = endian_swap16 (head->length);
  if (buf->debug & DEBUG_CGI)
    printf ("fcgi %d rec type %d id %d len %d\n", buf->fd, head->type, head->request_id, head->length);

  hin_fcgi_socket_t * sock = buf->parent;
  int req_id = head->request_id;
  int used = head->length;
  if (req_id < 0 || req_id > sock->max_worker) {
    printf ("fcgi %d req_id %d error! outside bounds\n", buf->fd, req_id);
    return used + 8 + head->padding;
  }
  hin_fcgi_worker_t * worker = sock->worker[req_id];

  if (head->type == FCGI_END_REQUEST) {
    hin_fcgi_pipe_end (worker, head);
  } else {
    hin_fcgi_pipe_write (worker, head);
  }
  return used + 8 + head->padding;
}

int hin_fcgi_read_callback (hin_buffer_t * buf, int ret) {
  hin_fcgi_socket_t * socket = buf->parent;
  if (ret < 0) {
    printf ("fcgi %d error! '%s'\n", buf->fd, strerror (-ret));
    hin_fcgi_socket_close (socket);
    return -1;
  }
  if (ret == 0) {
    if (buf->debug & DEBUG_CGI)
      printf ("fcgi %d close\n", buf->fd);
    hin_fcgi_socket_close (socket);
    return 1;
  }

  if (buf->debug & DEBUG_CGI)
    printf ("fcgi %d received %d bytes\n", buf->fd, ret);
  char * ptr = buf->ptr;
  int left = ret;
  int sz = 0;
  while ((sz = hin_fcgi_read_rec (buf, ptr, left)) > 0) {
    left -= sz;
    ptr += sz;
  }

  buf->count = buf->sz;
  if (hin_request_read (buf) < 0) {
    printf ("uring error!\n");
    return -1;
  }

  return 0;
}


