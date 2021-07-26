
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/un.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

#include "fcgi.h"

static hin_fcgi_group_t * fcgi_group = NULL;

void hin_fcgi_clean () {
  hin_fcgi_group_t * fcgi = fcgi_group;
  while (fcgi) {
    hin_fcgi_group_t * next = fcgi->next;

    if (fcgi->host) free (fcgi->host);
    if (fcgi->port) free (fcgi->port);
    if (fcgi->uri) free (fcgi->uri);
    free (fcgi);
    fcgi = next;
  }
  fcgi_group = NULL;
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
  fcgi->uri = strdup (uri);
  fcgi->magic = HIN_FCGI_MAGIC;

  fcgi->next = fcgi_group;
  fcgi_group = fcgi;

  return fcgi;
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

