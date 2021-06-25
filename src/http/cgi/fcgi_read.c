
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include <basic_endianness.h>

#include "fcgi.h"

static int hin_fcgi_write_callback (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    printf ("fcgi %d error! write '%s'\n", buf->fd, strerror (-ret));
    return -1;
  }
  hin_fcgi_worker_t * worker = buf->parent;
  hin_fcgi_socket_t * socket = worker->socket;
  if (buf->debug & DEBUG_CGI)
    printf ("fcgi %d worker %d done.\n", socket->fd, worker->req_id);
  hin_fcgi_worker_reset (worker);
  return 1;
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
  httpd_client_t * http = worker->http;
  // TODO

  if (head->type == FCGI_END_REQUEST) {
    if (buf->debug & DEBUG_CGI)
      printf ("fcgi %d req_id %d end\n", buf->fd, head->request_id);
    FCGI_EndRequestBody * req = (FCGI_EndRequestBody*)head->data;
    req->appStatus = endian_swap32 (req->appStatus);
    if (buf->debug & DEBUG_CGI)
      printf (" status app %d proto %d\n", req->appStatus, req->protocolStatus);
    // TODO request pipe end
  } else {
    printf ("payload '%.*s'\n", used, head->data);
    hin_buffer_t * buf = malloc (sizeof *buf + READ_SZ);
    memset (buf, 0, sizeof (*buf));
    buf->fd = http->c.sockfd;
    buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
    buf->sz = READ_SZ;
    buf->ptr = buf->buffer;
    buf->parent = worker;
    buf->debug = http->debug;
    buf->ssl = &http->c.ssl;
    buf->callback = hin_fcgi_write_callback;
    header (buf, "HTTP/1.1 200 OK\r\n");
    header_raw (buf, (char*)head->data, used);
    hin_request_write (buf);
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


