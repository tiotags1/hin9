
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include <basic_endianness.h>

#include "fcgi.h"

void hin_fcgi_worker_free (hin_fcgi_worker_t * worker) {
  hin_fcgi_socket_t * socket = worker->socket;
  if (socket) {
    socket->worker[worker->req_id] = NULL;

    socket->num_worker--;

    if (socket->num_worker <= 0)
      hin_fcgi_socket_close (socket);
  }
  free (worker);
}

hin_fcgi_worker_t * hin_fcgi_get_worker (hin_fcgi_group_t * fcgi) {
  if (fcgi->idle_worker.next) {
    basic_dlist_t * idle = fcgi->idle_worker.next;
    basic_dlist_remove (&fcgi->idle_worker, idle);
    hin_fcgi_worker_t * worker = basic_dlist_ptr (idle, offsetof (hin_fcgi_worker_t, list));
    worker->io_state &= ~HIN_REQ_END;
    return worker;
  }

  hin_fcgi_socket_t * sock = hin_fcgi_get_socket (fcgi);
  sock->num_worker++;

  int req_id = sock->last_worker++;
  if (sock->last_worker > sock->max_worker) {
    int new = 5;
    int max = sock->max_worker + new;
    sock->worker = realloc (sock->worker, sizeof (void*) * max);
    memset (&sock->worker[sock->max_worker], 0, sizeof (void*) * new);
    sock->max_worker = max;
  }

  hin_fcgi_worker_t * worker = calloc (1, sizeof (*worker));
  worker->req_id = req_id;
  worker->socket = sock;

  sock->worker[worker->req_id] = worker;

  return worker;
}

int hin_fcgi_worker_reset (hin_fcgi_worker_t * worker) {
  if ((worker->io_state & (HIN_REQ_POST|HIN_REQ_DATA|HIN_REQ_END))) return 0;
  worker->io_state |= HIN_REQ_END;

  httpd_client_t * http = worker->http;
  if (http) {
    httpd_client_finish_request (http, NULL);
    worker->http = NULL;
  }

  hin_fcgi_socket_t * socket = worker->socket;
  if (socket == NULL) {
    hin_fcgi_worker_free (worker);
    return 0;
  }

  hin_fcgi_group_t * fcgi = socket->fcgi;
  if (fcgi->socket) {
    basic_dlist_append (&fcgi->idle_worker, &worker->list);
  } else {
    hin_fcgi_socket_close (socket);
  }
  return 0;
}

void hin_fcgi_worker_run (hin_fcgi_worker_t * worker) {
  hin_fcgi_socket_t * socket = worker->socket;
  if (socket->fd < 0) {
    basic_dlist_append (&socket->que, &worker->list);
    return ;
  }

  hin_fcgi_write_request (worker);
}



