
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/un.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

#include "fcgi.h"

void hin_fcgi_socket_close (hin_fcgi_socket_t * socket) {
  if (master.debug & DEBUG_CGI)
    printf ("fcgi %d close\n", socket->fd);

  for (int i=0; i<socket->max_worker; i++) {
    hin_fcgi_worker_t * worker = socket->worker[i];
    if (worker == NULL) continue;
    if (worker->http) {
      httpd_error (worker->http, 500, "fcgi socket closed");
    }
    worker->socket = NULL;
    hin_fcgi_worker_reset (worker);
  }
  if (socket->worker) free (socket->worker);
  socket->worker = NULL;
  if (socket->fd > 0)
    close (socket->fd);
  socket->fd = -1;

  master.num_connection--;

  free (socket);
}

static int hin_fcgi_eat_callback (hin_buffer_t * buffer, int num) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;

  if (num > 0) {
    hin_buffer_eat (buffer, num);
    hin_lines_request (buffer, buffer->count);
  } else if (num == 0) {
    hin_lines_request (buffer, buffer->count);
  } else {
    if (lines->close_callback) {
      return lines->close_callback (buffer, num);
    } else {
      printf ("lines client error %d\n", num);
    }
    return -1;
  }
  return 0;
}

static int hin_fcgi_connect_callback (hin_buffer_t * buf, int ret) {
  hin_fcgi_socket_t * socket = buf->parent;
  hin_fcgi_group_t * fcgi = socket->fcgi;

  if (ret < 0) {
    printf ("fcgi socket failed '%s' '%s'\n", fcgi->uri, strerror (-ret));
    hin_fcgi_socket_close (socket);
    return -1;
  }

  socket->fd = buf->fd;

  if (buf->debug & DEBUG_CGI)
    printf ("fcgi %d worker connected '%s'\n", socket->fd, fcgi->uri);

  if (socket->queued) {
    hin_fcgi_worker_t * worker = socket->queued;
    hin_fcgi_write_request (worker);
  }

  hin_buffer_t * buf1 = hin_lines_create_raw (READ_SZ);
  buf1->fd = buf->fd;
  buf1->parent = socket;
  buf1->flags = 0;
  buf1->debug = master.debug;
  hin_lines_t * lines = (hin_lines_t*)&buf1->buffer;
  int hin_fcgi_read_callback (hin_buffer_t * buf, int ret);
  lines->read_callback = hin_fcgi_read_callback;
  lines->eat_callback = hin_fcgi_eat_callback;
  if (hin_request_read (buf1) < 0) {
    hin_fcgi_socket_close (socket);
    return -1;
  }

  return 1;
}

hin_fcgi_socket_t * hin_fcgi_get_socket (hin_fcgi_group_t * fcgi) {
  // TODO try to get a reusable sockets

  hin_fcgi_socket_t * sock = calloc (1, sizeof (*sock));
  sock->fd = -1;
  sock->fcgi = fcgi;

  hin_connect (fcgi->host, fcgi->port, hin_fcgi_connect_callback, sock, &sock->ai_addr, &sock->ai_addrlen);

  return sock;
}


