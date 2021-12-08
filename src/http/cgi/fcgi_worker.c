
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include <basic_endianness.h>

#include "fcgi.h"

void hin_fcgi_worker_free (hin_fcgi_worker_t * worker) {
  free (worker);
}

hin_fcgi_worker_t * hin_fcgi_get_worker (hin_fcgi_group_t * fcgi) {
  if (fcgi->free) {
    //hin_fcgi_worker_t * worker = fcgi->free;
    // TODO add to busy pool
    //return worker;
  }

  hin_fcgi_socket_t * sock = hin_fcgi_get_socket (fcgi);

  int req_id = sock->num_worker++;
  if (sock->num_worker > sock->max_worker) {
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

  worker->io_state |= HIN_REQ_DATA;

  return worker;
}

int hin_fcgi_worker_reset (hin_fcgi_worker_t * worker) {
  if ((worker->io_state & (HIN_REQ_POST|HIN_REQ_DATA))) return 0;

  httpd_client_t * http = worker->http;
  if (http) {
    //httpd_client_finish_request (http);
    //worker->http = NULL;
  }
  if (worker->socket == NULL) {
    hin_fcgi_worker_free (worker);
    return 0;
  } else {
    hin_fcgi_socket_close (worker->socket);
  }

  // TODO if not null then add to pool
  return 0;
}

void hin_fcgi_worker_run (hin_fcgi_worker_t * worker) {
  hin_fcgi_socket_t * socket = worker->socket;
  if (socket->fd < 0) {
    if (socket->queued) {
      // TODO lazy atm, should queue
      httpd_error (worker->http, 501, "fcgi worker should queue");
      return ;
    }
    socket->queued = worker;
    return ;
  }

  hin_fcgi_write_request (worker);
}



