
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <basic_pattern.h>

#include "hin.h"
#include "http.h"
#include "file.h"
#include "conf.h"
#include "vhost.h"

int httpd_write_common_headers (httpd_client_t * http, hin_buffer_t * buf) {
  if ((http->disable & HIN_HTTP_DATE) == 0) {
    time_t rawtime;
    time (&rawtime);
    header_date (buf, "Date: " HIN_HTTP_DATE_FORMAT "\r\n", rawtime);
  }
  if ((http->disable & HIN_HTTP_BANNER) == 0) {
    header (buf, "Server: %s\r\n", HIN_HTTPD_SERVER_BANNER);
  }
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    header (buf, "Content-Encoding: deflate\r\n");
  } else if (http->peer_flags & HIN_HTTP_GZIP) {
    header (buf, "Content-Encoding: gzip\r\n");
  }
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (buf, "Transfer-Encoding: chunked\r\n");
  }
  if ((http->disable & HIN_HTTP_CACHE) == 0 && http->cache_flags) {
    header_cache_control (buf, http->cache_flags, http->cache);
  }
  if (http->peer_flags & HIN_HTTP_KEEPALIVE) {
    header (buf, "Connection: keep-alive\r\n");
  } else {
    header (buf, "Connection: close\r\n");
  }
  if (http->content_type) {
    header (buf, "Content-Type: %s\r\n", http->content_type);
  }
  httpd_vhost_t * vhost = http->vhost;
  if (vhost->hsts && (vhost->vhost_flags & HIN_HSTS_NO_HEADER) == 0) {
    header (buf, "Strict-Transport-Security: max-age=%d", vhost->hsts);
    if (vhost->vhost_flags & HIN_HSTS_SUBDOMAINS)
      header (buf, "; includeSubDomains");
    if (vhost->vhost_flags & HIN_HSTS_PRELOAD)
      header (buf, "; preload");
    header (buf, "\r\n");
  }
  if (http->append_headers) {
    header (buf, "%s", http->append_headers);
  }
  return 0;
}

static int http_raw_response_callback (hin_buffer_t * buffer, int ret) {
  httpd_client_t * http = (httpd_client_t*)buffer->parent;

  if (ret < 0) {
    printf ("httpd sending error %s\n", strerror (-ret));
  } else if (hin_buffer_continue_write (buffer, ret) > 0) {
    http->count += ret;
    return 0;
  }

  http->count += ret;

  http->state &= ~HIN_REQ_ERROR;
  httpd_client_finish_request (http, NULL);

  return 1;
}

int httpd_respond_text (httpd_client_t * http, int status, const char * body) {
  hin_client_t * client = &http->c;

  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= HIN_REQ_DATA;

  if (http->method & HIN_METHOD_POST) {
    http->method = HIN_METHOD_GET;
    httpd_error (http, 405, "POST on a raw resource");
    return 0;
  }
  http->status = status;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = http_raw_response_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;
  buf->debug = http->debug;

  int freeable = 0;
  if (body == NULL) {
    freeable = 1;
    if (asprintf ((char**)&body, "<html><head></head><body><h1>Error %d: %s</h1></body></html>\n", status, http_status_name (status)) < 0)
      perror ("asprintf");
  }
  http->disable |= HIN_HTTP_CHUNKED | HIN_HTTP_COMPRESS | HIN_HTTP_CACHE;
  http->peer_flags &= ~ http->disable;
  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));
  httpd_write_common_headers (http, buf);
  header (buf, "Content-Length: %ld\r\n", strlen (body));
  header (buf, "\r\n");
  if (http->method != HIN_METHOD_HEAD) {
    header (buf, "%s", body);
  }
  if (freeable) free ((char*)body);
  if (http->debug & DEBUG_RW) printf ("httpd %d raw response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);
  hin_request_write (buf);
  return 0;
}

int httpd_respond_error (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_ERROR) return 0;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_POST);
  http->state |= HIN_REQ_ERROR;
  http->method = HIN_METHOD_GET;
  return httpd_respond_text (http, status, body);
}

int httpd_respond_fatal (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_ERROR) return 0;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_POST);
  http->state |= HIN_REQ_ERROR;
  http->method = HIN_METHOD_GET;
  httpd_respond_text (http, status, body);
  httpd_client_shutdown (http);
  return 0;
}

int httpd_respond_fatal_and_full (httpd_client_t * http, int status, const char * body) {
  if (http->state & HIN_REQ_ERROR) return 0;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_POST);
  http->state |= HIN_REQ_ERROR;
  http->method = HIN_METHOD_GET;
  httpd_respond_text (http, status, body);
  httpd_client_shutdown (http);
  return 0;
}


int httpd_respond_buffer (httpd_client_t * http, int status, hin_buffer_t * data) {
  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= HIN_REQ_DATA;
  http->status = status;

  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = http_raw_response_callback;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  off_t len = 0;
  for (basic_dlist_t * elem = &data->list; elem; elem = elem->next) {
    hin_buffer_t * buf = hin_buffer_list_ptr (elem);
    len += buf->count;
  }

  http->disable |= HIN_HTTP_CHUNKED | HIN_HTTP_COMPRESS | HIN_HTTP_CACHE;
  http->peer_flags &= ~ http->disable;
  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));
  httpd_write_common_headers (http, buf);
  header (buf, "Content-Length: %ld\r\n", len);
  header (buf, "\r\n");
  if (http->debug & DEBUG_RW) printf ("httpd %d buffer response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);

  if (http->method != HIN_METHOD_HEAD) {
    basic_dlist_t * elem = &buf->list;
    while (elem->next) elem = elem->next;
    basic_dlist_add_after (NULL, elem, &data->list);
  } else {
    basic_dlist_t * elem = &data->list;
    while (elem) {
      hin_buffer_t * buf1 = hin_buffer_list_ptr (elem);
      elem = elem->next;
      hin_buffer_clean (buf1);
    }
  }

  hin_request_write (buf);
  return 0;
}

int httpd_respond_redirect (httpd_client_t * http, int status, const char * location) {
  char * new = NULL;
  char * old = http->append_headers;
  int num = asprintf (&new, "Location: %s\r\n%s", location, old ? old : "");
  if (num < 0) { if (new) free (new); return -1; }

  if (old) free (old);
  http->append_headers = new;
  httpd_respond_text (http, status ? status : 302, "");
  return 0;
}

int httpd_respond_redirect_https (httpd_client_t * http) {
  string_t source, line, path;
  source = http->headers;
  if (hin_find_line (&source, &line) == 0 || hin_http_parse_header_line (&line, NULL, &path, NULL) < 0) {
    return -1;
  }

  char * new = NULL;
  int num = asprintf (&new, "https://%s%.*s", http->hostname, (int)path.len, path.ptr);
  if (num < 0) { if (new) free (new); return -1; }

  httpd_respond_redirect (http, 302, new);
  free (new);
  return 0;
}



