
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

void http_client_clean (http_client_t * http) {
  if (http->save_path) free ((void*)http->save_path);
  if (http->uri.all.ptr) free ((void*)http->uri.all.ptr);
  memset (http, 0, sizeof (*http));
}

int http_client_start_request (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client->extra;
  if (ret < 0) {
    printf ("can't connect '%s'\n", strerror (-ret));
    return -1;
  }
  if (master.debug & DEBUG_PROTO) printf ("http request begin on socket %d\n", ret);
  //
  int http_send_request (hin_client_t * client);
  http_send_request (client);
}

int http_client_finish_request (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("http request done\n");
  hin_client_shutdown (client);
}

int http_client_error (hin_client_t * client) {
  printf ("http error !!!\n");
}

int http_client_close (hin_client_t * client) {
  if (master.debug & DEBUG_PROTO) printf ("http close client\n");
}

static int connected (hin_client_t * client, int ret) {
  http_client_t * http = (http_client_t*)client->extra;
  if (ret < 0) {
    return -1;
  }
  if (http->uri.https) {
    hin_connect_ssl_init (client);
  }
  //int http_client_start_request (hin_client_t * client, int);
  http_client_start_request (client, ret);
}

hin_client_t * http_download (const char * url1, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush)) {
  printf ("http download '%s' to '%s'\n", url1, save_path);
  char * url = strdup (url1);
  hin_uri_t info;
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url1);
    return NULL;
  }

  char * h = strndup (info.host.ptr, info.host.len);
  char * p = NULL;
  if (info.port.len > 0) {
    p = strndup (info.port.ptr, info.port.len);
  } else {
    p = strdup ("80");
  }
  hin_client_t * client = hin_connect (h, p, sizeof (http_client_t), &connected);
  free (h);
  free (p);
  http_client_t * http = (http_client_t*)client->extra;
  http->save_path = strdup (save_path);
  http->uri = info;
  return client;
}



