
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/un.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

#include "fcgi.h"

static hin_fcgi_group_t * fcgi_group = NULL;

static void hin_fcgi_group_clean (hin_fcgi_group_t * fcgi) {
  hin_dlist_t * elem = fcgi->idle_worker.next;
  while (elem) {
    hin_dlist_t * next = elem->next;
    hin_fcgi_worker_t * worker = hin_dlist_ptr (elem, offsetof (hin_fcgi_worker_t, list));
    hin_fcgi_worker_free (worker);
    elem = next;
  }

  if (fcgi->socket) free (fcgi->socket);
  if (fcgi->host) free (fcgi->host);
  if (fcgi->port) free (fcgi->port);
  if (fcgi->uri) free (fcgi->uri);
}

void hin_fcgi_clean () {
  hin_fcgi_group_t * fcgi = fcgi_group;
  while (fcgi) {
    hin_fcgi_group_t * next = fcgi->next;
    hin_fcgi_group_clean (fcgi);
    free (fcgi);
    fcgi = next;
  }
  fcgi_group = NULL;
}

hin_fcgi_group_t * hin_fcgi_start (const char * uri, int min, int max) {
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
  fcgi->uri = strdup (uri);
  fcgi->magic = HIN_FCGI_MAGIC;
  fcgi->min_socket = min;
  fcgi->max_socket = max;
  if (max) {
    fcgi->socket = calloc (1, sizeof (void*) * max);
  }

  if (master.debug & (DEBUG_CGI|DEBUG_CONFIG))
    printf ("fcgi group '%s' min %d max %d\n", uri, min, max);

  fcgi->next = fcgi_group;
  fcgi_group = fcgi;

  return fcgi;
}

int hin_fastcgi (httpd_client_t * http, void * fcgi1, const char * script_path, const char * path_info) {
  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= (HIN_REQ_DATA | HIN_REQ_FCGI);

  hin_fcgi_group_t * fcgi = fcgi1;
  if (fcgi == NULL || fcgi->magic != HIN_FCGI_MAGIC) {
    httpd_error (http, 500, "fcgi invalid");
    return -1;
  }

  if (!HIN_HTTPD_CGI_CHUNKED_UPLOAD && (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD)) {
    httpd_error (http, 411, "cgi spec denies chunked upload");
    return -1;
  }

  if (hin_cache_check (NULL, http) > 0) {
    return 0;
  }

  hin_fcgi_worker_t * worker = hin_fcgi_get_worker (fcgi);
  if (worker == NULL) {
    httpd_error (http, 500, "worker null");
    return -1;
  }
  worker->http = http;
  httpd_request_chunked (http);

  hin_fcgi_worker_run (worker);

  return 0;
}

