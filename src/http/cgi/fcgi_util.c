
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

static hin_fcgi_socket_t * hin_fcgi_create_socket (hin_fcgi_group_t * fcgi) {
  hin_fcgi_socket_t * sock = calloc (1, sizeof (*sock));
  sock->fd = -1;
  sock->fcgi = fcgi;
  return sock;
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

  int new = 10;
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

FCGI_Header * hin_fcgi_header (hin_buffer_t * buf, int type, int id, int sz) {
  FCGI_Header * head = header_ptr (buf, sizeof (*head));
  head->version = FCGI_VERSION_1;
  head->type = type;
  head->request_id = endian_swap16 (id);
  head->length = endian_swap16 (sz);
  head->padding = 0;
  if (buf->debug & DEBUG_CGI)
    printf ("fcgi type %d req %d sz %d\n", type, id, sz);
  return head;
}



