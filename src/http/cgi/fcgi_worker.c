
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

hin_fcgi_worker_t * hin_fcgi_worker_create (hin_fcgi_socket_t * sock) {
  hin_fcgi_worker_t * worker = calloc (1, sizeof (*worker));
  worker->req_id = sock->next_req++;
  worker->socket = sock;
  return worker;
}

hin_fcgi_worker_t * hin_fcgi_get_worker (hin_fcgi_group_t * fcgi) {
  hin_fcgi_worker_t * worker = NULL;
  hin_fcgi_socket_t * sock = hin_fcgi_create_socket (fcgi);
  for (int i=0; i<sock->max_worker; i++) {
    worker = sock->worker[i];
    if (worker) continue;
    worker = hin_fcgi_worker_create (sock);
    sock->worker[i] = worker;
    return worker;
  }

  int new = 1; // TODO low number is temp for single client per connection
  int max = sock->max_worker + new;
  sock->worker = realloc (sock->worker, sizeof (void*) * max);
  memset (&sock->worker[sock->max_worker], 0, sizeof (void*) * new);
  worker = hin_fcgi_worker_create (sock);
  sock->worker[sock->max_worker] = worker;
  sock->max_worker = max;
  return worker;
}

int hin_fcgi_worker_reset (hin_fcgi_worker_t * worker) {
  httpd_client_t * http = worker->http;
  if (http) {
    //httpd_client_finish_request (http);
    worker->http = NULL;
  }
  return 0;
}

void hin_fcgi_worker_run (hin_fcgi_worker_t * worker) {
  hin_fcgi_socket_t * socket = worker->socket;
  if (socket->fd < 0) {
    if (socket->queued) {
      // TODO lazy atm, should queue
      httpd_respond_error (worker->http, 501, "lazy");
      return ;
    }
    socket->queued = worker;
    return ;
  }

  hin_fcgi_write_request (worker);
}



