
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/un.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

#include "fcgi.h"

static hin_fcgi_group_t * fcgi_group = NULL;

static int hin_fcgi_connect_callback (hin_buffer_t * buf, int ret) {
  const char * path = NULL;
  hin_fcgi_socket_t * socket = buf->parent;
  hin_fcgi_group_t * fcgi = socket->fcgi;

  if (0) {
    struct sockaddr_un * sock = (struct sockaddr_un *)buf->buffer;
    path = sock->sun_path;
  } else {
    path = fcgi->host;
  }
  if (ret < 0) {
    printf ("fcgi socket failed '%s' '%s'\n", path, strerror (-ret));
    return -1;
  }

  socket->fd = buf->fd;

  if (buf->debug & DEBUG_CGI)
    printf ("fcgi %d worker connected '%s'\n", socket->fd, path);

  if (socket->queued) {
    hin_fcgi_worker_t * worker = socket->queued;
    hin_fcgi_write_request (worker);
  }

  return 1;
}

static int hin_fcgi_socket_connect (hin_fcgi_socket_t * socket) {
  hin_fcgi_group_t * fcgi = socket->fcgi;
  hin_connect (fcgi->host, fcgi->port, hin_fcgi_connect_callback, socket, &socket->ai_addr, &socket->ai_addrlen);
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
    hin_fcgi_socket_connect (socket);
    return ;
  }

  hin_fcgi_write_request (worker);
}

int hin_fastcgi (httpd_client_t * http, void * fcgi1, const char * script_path, const char * path_info) {
  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= (HIN_REQ_DATA | HIN_REQ_FCGI);

  hin_fcgi_group_t * fcgi = fcgi1;
  if (fcgi == NULL || fcgi->magic != HIN_FCGI_MAGIC) {
    printf ("error! fcgi group is invalid\n");
    httpd_respond_fatal (http, 500, NULL);
    return -1;
  }

  if (!HIN_HTTPD_CGI_CHUNKED_UPLOAD && (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD)) {
    printf ("cgi spec denies chunked upload\n");
    httpd_respond_fatal (http, 411, NULL);
    return -1;
  }

  if (hin_cache_check (NULL, http) > 0) {
    return 0;
  }

  hin_fcgi_worker_t * worker = hin_fcgi_get_worker (fcgi);
  if (worker == NULL) {
    printf ("error! worker null\n");
    return -1;
  }
  worker->http = http;
  httpd_request_chunked (http);

  hin_fcgi_worker_run (worker);

  return 0;
}

hin_fcgi_group_t * hin_fcgi_start (const char * uri) {
  string_t source, host, port;
  source.ptr = (char*)uri;
  source.len = strlen (source.ptr);
  if (match_string (&source, "tcp://") <= 0) {
    printf ("error! fcgi missing 'tcp://' '%s'\n", uri);
    return NULL;
  }

  if (match_string (&source, "([%w]+)", &host) <= 0) {
    printf ("error! fcgi missing host '%s'\n", uri);
    return NULL;
  }
  if (match_string (&source, ":([%d]+)", &port) <= 0) {
    printf ("error! fcgi missing port '%s'\n", uri);
    return NULL;
  }

  hin_fcgi_group_t * fcgi = calloc (1, sizeof (*fcgi));
  fcgi->host = strndup (host.ptr, host.len);
  fcgi->port = strndup (port.ptr, port.len);
  fcgi->magic = HIN_FCGI_MAGIC;

  fcgi->next = fcgi_group;
  fcgi_group = fcgi;

  return fcgi;
}

void hin_fcgi_socket_close (hin_fcgi_socket_t * socket) {
  for (int i=0; i<socket->max_worker; i++) {
    hin_fcgi_worker_t * worker = socket->worker[i];
    if (worker == NULL) continue;
    //hin_fcgi_worker_reset (worker);
    free (worker);
  }
  if (socket->path) free ((void*)socket->path);
  if (socket->worker) free (socket->worker);
  if (socket->fd > 0)
    close (socket->fd);
  socket->fd = -1;
  socket->worker = NULL;

  hin_fcgi_group_t * fcgi = socket->fcgi;
  fcgi->socket = NULL;
  free (socket);
}

void hin_fcgi_clean () {
  hin_fcgi_group_t * fcgi = fcgi_group;
  while (fcgi) {
    hin_fcgi_group_t * next = fcgi->next;

    hin_fcgi_socket_t * socket = fcgi->socket;
    if (socket)
      hin_fcgi_socket_close (socket);

    if (fcgi->host) free (fcgi->host);
    if (fcgi->port) free (fcgi->port);
    free (fcgi);
    fcgi = next;
  }
  fcgi_group = NULL;
}


