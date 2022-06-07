
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include <basic_endianness.h>

#include "fcgi.h"

int hin_fcgi_pipe_init (hin_fcgi_worker_t * worker) {
  httpd_client_t * http = worker->http;

  hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
  buf->fd = -1;
  buf->parent = worker;
  buf->flags = 0;
  buf->debug = http->debug;

  worker->header_buf = buf;

  int hin_fcgi_send (httpd_client_t * http, hin_buffer_t * buf);
  hin_fcgi_send (http, buf);

  return 0;
}

static int hin_fcgi_pipe_write (hin_fcgi_worker_t * worker, FCGI_Header * head) {
  if (worker->header_buf) {
    hin_lines_write (worker->header_buf, (char*)head->data, head->length);
  } else {
    hin_buffer_t * buf = hin_buffer_create_from_data (worker->out, (char*)head->data, head->length);
    httpd_client_t * http = worker->http;
    buf->debug = http->debug;
    hin_pipe_write_process (worker->out, buf, HIN_PIPE_ALL);
    hin_pipe_advance (worker->out);
  }
  return 0;
}

static int hin_fcgi_pipe_end (hin_fcgi_worker_t * worker, FCGI_Header * head) {
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = worker->http;
  FCGI_EndRequestBody * req = (FCGI_EndRequestBody*)head->data;
  req->appStatus = endian_swap32 (req->appStatus);
  if (http->debug & DEBUG_CGI)
    printf ("httpd %d fcgi %d req_id %d status %d proto %d end\n", http->c.sockfd, socket->fd, head->request_id, req->appStatus, req->protocolStatus);

  if (req->protocolStatus != FCGI_REQUEST_COMPLETE || req->appStatus) {
    printf ("httpd %d fcgi %d finish error\n", http->c.sockfd, socket->fd);
  }

  hin_pipe_t * pipe = worker->out;
  hin_pipe_finish (pipe);
  worker->out = NULL;

  return 0;
}

int hin_fcgi_read_rec (hin_buffer_t * buf, char * ptr, int left) {
  FCGI_Header * head = (FCGI_Header*)ptr;
  if (left < (int)sizeof (*head)) return 0;
  int len = endian_swap16 (head->length);
  int total = len + sizeof (*head) + head->padding;
  if (left < total) {
    if (buf->debug & DEBUG_CGI)
      printf ("fcgi %d request more %d<%d\n", buf->fd, left, total);
    buf->count = total; // hint to set request size
    return 0;
  }

  head->request_id = endian_swap16 (head->request_id);
  head->length = len;

  hin_fcgi_socket_t * sock = buf->parent;
  int req_id = head->request_id;
  if (req_id < 0 || req_id > sock->max_worker) {
    printf ("fcgi %d req_id %d error! outside bounds\n", buf->fd, req_id);
    return total;
  }
  hin_fcgi_worker_t * worker = sock->worker[req_id];

  if (buf->debug & DEBUG_CGI)
    printf ("httpd %d fcgi %d rec type %d id %d len %d\n", worker->http->c.sockfd, buf->fd, head->type, head->request_id, head->length);

  switch (head->type) {
  case FCGI_STDOUT:
    hin_fcgi_pipe_write (worker, head);
  break;
  case FCGI_STDERR:
    fprintf (stderr, "fcgi %d error %d '%.*s'\n", buf->fd, len, len, head->data);
    // TODO how do you get separate messages from this ?
  break;
  case FCGI_END_REQUEST:
    hin_fcgi_pipe_end (worker, head);
  break;
  default:
    printf ("fcgi %d unkown request type %d\n", buf->fd, head->type);
  }
  return total;
}

int hin_fcgi_socket_read_callback (hin_buffer_t * buf, int ret) {
  hin_fcgi_socket_t * socket = buf->parent;
  if (ret < 0) {
    printf ("fcgi %d error! '%s'\n", buf->fd, strerror (-ret));
    hin_fcgi_socket_close (socket);
    return 0;
  }
  if (ret == 0) {
    if (buf->debug & DEBUG_CGI)
      printf ("fcgi %d eof\n", buf->fd);
    hin_fcgi_socket_close (socket);
    return 0;
  }

  if (buf->debug & DEBUG_CGI)
    printf ("fcgi %d received %d bytes\n", buf->fd, ret);

  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  char * ptr = lines->base;
  int left = lines->count;
  int sz = 0;
  int used = 0;

  while ((sz = hin_fcgi_read_rec (buf, ptr, left)) > 0) {
    left -= sz;
    ptr += sz;
    used += sz;
  }

  return used;
}


