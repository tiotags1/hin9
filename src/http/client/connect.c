
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

void http_client_clean (http_client_t * http) {
  if (http->debug & DEBUG_MEMORY) printf ("http %d clean\n", http->c.sockfd);
  if (http->uri.all.ptr) {
    free ((void*)http->uri.all.ptr);
    http->uri.all.ptr = NULL;
  }
  if (http->save_fd) { // TODO should it clean save_fd ?
    if (http->debug & DEBUG_SYSCALL) printf ("  close save_fd %d\n", http->save_fd);
    close (http->save_fd);
    http->save_fd = 0;
  }
  http->c.parent = NULL;

  http->io_state &= HIN_REQ_HEADERS;
}

int http_connection_allocate (http_client_t * http) {
  if (http->io_state & HIN_REQ_IDLE)
    basic_dlist_remove (&master.connection_idle, &http->c.list);
  http->io_state &= ~HIN_REQ_IDLE;

  return 0;
}

int http_connection_release (http_client_t * http) {
  if ((http->flags & HIN_HTTP_KEEPALIVE) == 0) {
    http_client_shutdown (http);
    return 0;
  }

  if (http->debug & DEBUG_HTTP)
    printf ("http %d idled\n", http->c.sockfd);

  basic_dlist_append (&master.connection_idle, &http->c.list);
  http_client_clean (http);
  http->io_state |= HIN_REQ_IDLE;

  if (hin_lines_request (http->read_buffer, 0)) {
    printf ("error! %d\n", 943547909);
    return 0;
  }

  return 0;
}

int http_client_unlink (http_client_t * http) {
  if ((http->io_state & HIN_REQ_END) == 0) { return 0; }
  if ((http->io_state & HIN_REQ_HEADERS)) { return 0; }
  if ((http->read_buffer->flags & HIN_ACTIVE)) { return 0; }

  if (http->debug & DEBUG_HTTP)
    printf ("http %d unlink\n", http->c.sockfd);

  hin_connect_release (http->c.sockfd);

  http_client_clean (http);
  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = http->port = NULL;

  if (http->read_buffer) {
    hin_buffer_clean (http->read_buffer);
    http->read_buffer = NULL;
  }

  free (http);
  hin_check_alive ();

  return 1;
}

static int http_client_close_callback (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;
  if (ret < 0) {
    printf ("http %d client close callback error: %s\n", http->c.sockfd, strerror (-ret));
    return -1;
  }
  hin_buffer_clean (buffer);
  http->io_state |= HIN_REQ_END;
  http_client_unlink (http);
  return 0;
}

int http_client_shutdown (http_client_t * http) {
  if (http_client_unlink (http)) return 0;

  if (http->io_state & HIN_REQ_STOPPING) return 0;
  http->io_state |= HIN_REQ_STOPPING;

  if (http->io_state & HIN_REQ_IDLE)
    basic_dlist_remove (&master.connection_idle, &http->c.list);

  if (http->debug & DEBUG_HTTP) printf ("http %d shutdown\n", http->c.sockfd);

  hin_buffer_t * buf = malloc (sizeof *buf);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->callback = http_client_close_callback;
  buf->parent = &http->c;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  if (hin_request_close (buf) < 0) {
    buf->flags |= HIN_SYNC;
    hin_request_close (buf);
  }

  return 0;
}

void http_connection_close_idle () {
  basic_dlist_t * elem = master.connection_idle.next;
  while (elem) {
    http_client_t * http = basic_dlist_ptr (elem, offsetof (hin_client_t, list));
    elem = elem->next;

    http_client_shutdown (http);
  }
}

int http_client_finish_request (http_client_t * http) {
  if (http->debug & DEBUG_HTTP) printf ("http %d request done\n", http->c.sockfd);

  http_connection_release (http);

  return 0;
}

static int str_equal (const char * a, string_t * b) {
  const char * ptr = b->ptr;
  const char * max = b->ptr + b->len;
  while (1) {
    if (ptr >= max) break;
    if (*a != *ptr) return 0;
    a++; ptr++;
  }
  if (*a == '\0') return 1;
  return 0;
}

http_client_t * http_connection_get (const char * url1) {
  http_client_t * http = NULL;

  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    fprintf (stderr, "can't parse uri '%s'\n", url);
    free (url);
    return NULL;
  }

  basic_dlist_t * elem = master.connection_idle.next;
  while (elem) {
    http_client_t * http1 = basic_dlist_ptr (elem, offsetof (hin_client_t, list));
    elem = elem->next;

    if (http1->io_state & HIN_REQ_HEADERS) continue;
    if (str_equal (http1->host, &info.host) == 0) continue;
    if (str_equal (http1->port, &info.port) == 0) continue;
    http = http1;
    break;
  }

  if (http == NULL) {
    http = calloc (1, sizeof (*http));
    http->host = strndup (info.host.ptr, info.host.len);
    if (info.port.len > 0) {
      http->port = strndup (info.port.ptr, info.port.len);
    } else {
      http->port = strdup ("80");
    }
    http->c.sockfd = -1;

  } else {
    http_connection_allocate (http);
  }

  http->uri = info;

  return http;
}

