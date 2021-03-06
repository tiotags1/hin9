
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>

#include <basic_vfs.h>

#include "hin.h"
#include "http.h"
#include "uri.h"
#include "conf.h"
#include "vhost.h"
#include "file.h"

#include "fcgi.h"

#ifdef HIN_USE_CGI

static int hin_pipe_cgi_server_finish_callback (hin_pipe_t * pipe) {
  hin_fcgi_worker_t * worker = (hin_fcgi_worker_t *)pipe->parent1;
  if (pipe->debug & (DEBUG_CGI|DEBUG_PIPE))
    printf ("pipe %d>%d cgi transfer finished bytes %lld\n", pipe->in.fd, pipe->out.fd, (long long)pipe->count);

  if (pipe->debug & (DEBUG_CGI|DEBUG_SYSCALL))
    printf ("  cgi read done, close %d\n", pipe->in.fd);
  close (pipe->in.fd);

  #if HIN_HTTPD_WORKER_PREFORKED
  hin_worker_reset (worker);
  #else
  free (worker);
  #endif

  return httpd_client_finish_request (pipe->parent, pipe);
}

#endif

#ifdef HIN_USE_FCGI

static int hin_fcgi_pipe_finish_callback (hin_pipe_t * pipe) {
  hin_fcgi_worker_t * worker = pipe->parent1;
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = worker->http;
  if (http && http->debug & DEBUG_CGI)
    printf ("httpd %d fcgi %d worker %d done.\n", http->c.sockfd, socket->fd, worker->req_id);

  http->state &= ~HIN_REQ_FCGI;

  worker->http = NULL;
  worker->io_state &= ~HIN_REQ_DATA;
  hin_fcgi_worker_reset (worker);

  httpd_client_t * http1 = pipe->parent;
  return httpd_client_finish_request (http1, pipe);
}

#endif

#if defined (HIN_USE_CGI) || defined (HIN_USE_FCGI)

static int hin_cgi_location (httpd_client_t * http, hin_buffer_t * buf, string_t * origin) {
  string_t source = *origin;
  //if (http->status == 200) http->status = 302; // bad idea ?
  if (match_string (&source, "http") > 0) {
    header (buf, "Location: %.*s\r\n", origin->len, origin->ptr);
    return 0;
  }
  if (master.debug & DEBUG_CGI)
    fprintf (stderr, "cgi %d location2 %.*s\n", http->c.sockfd, (int)origin->len, origin->ptr);
  header (buf, "Location: %.*s\r\n", origin->len, origin->ptr);
  return 0;
}

static int hin_cgi_headers_read_callback (hin_buffer_t * buffer, int received) {
  hin_fcgi_worker_t * worker = (hin_fcgi_worker_t *)buffer->parent;
  httpd_client_t * http = worker->http;
  string_t source1, * source = &source1;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  source->ptr = lines->base;
  source->len = lines->count;

  string_t line, orig=*source, param1;
  http->status = 200;
  off_t sz = 0;
  uint32_t disable = 0;
  while (1) {
    if (hin_find_line (source, &line) == 0) { return 0; }
    if (line.len == 0) break;
    if (matchi_string (&line, "Status: (%d+)", &param1) > 0) {
      http->status = atoi (param1.ptr);
      if (http->debug & DEBUG_CGI) printf ("cgi %d status is %d\n", http->c.sockfd, http->status);
    } else if (matchi_string_equal (&line, "Content-Length: (%d+)", &param1) > 0) {
      sz = atoi (param1.ptr);
      http->count = sz;
    } else if (matchi_string (&line, "Content-Encoding:") > 0) {
      disable |= HIN_HTTP_COMPRESS;
    } else if (matchi_string (&line, "Transfer-Encoding:") > 0) {
      disable |= HIN_HTTP_CHUNKED;
    } else if (matchi_string (&line, "Cache-Control:") > 0) {
      disable |= HIN_HTTP_CACHE;
      httpd_parse_cache_str (line.ptr, line.len, &http->cache_flags, &http->cache);
    } else if (matchi_string (&line, "Date:") > 0) {
      disable |= HIN_HTTP_DATE;
    }
  }

  int len = source->len;
  if (sz && sz < len) len = sz;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  hin_pipe_init (pipe);
  pipe->in.fd = buffer->fd;
  pipe->in.flags = 0;
  pipe->in.pos = 0;
  pipe->parent = http;
  pipe->parent1 = worker;

  if (worker->socket) {
    pipe->in.flags = HIN_INACTIVE;
    #ifdef HIN_USE_FCGI
    pipe->finish_callback = hin_fcgi_pipe_finish_callback;
    #endif
    worker->out = pipe;
    worker->header_buf = NULL;
  } else {
    #ifdef HIN_USE_CGI
    pipe->finish_callback = hin_pipe_cgi_server_finish_callback;
    #endif
  }

  http->disable |= disable;
  http->peer_flags = http->peer_flags & (~http->disable);

  if ((http->cache_flags & HIN_CACHE_PUBLIC) && http->method != HIN_METHOD_POST && http->status == 200) {
    // cache check is somewhere else
    int n = hin_cache_save (NULL, pipe);
    if (n == 0) {
      // pipe already started ?
      free (pipe);
      return -1;
    } else if (n > 0) {
      if (len > 0) {
        hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
        hin_pipe_write_process (pipe, buf1, HIN_PIPE_ALL);
      }
      hin_pipe_start (pipe);
      return -1;
    }
  }

  httpd_pipe_set_http11_response_options (http, pipe);

  int sz1 = source->len + READ_SZ;
  hin_buffer_t * buf = malloc (sizeof (*buf) + sz1);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->count = 0;
  buf->sz = sz1;
  buf->ptr = buf->buffer;
  buf->parent = pipe;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  *source = orig;
  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));

  httpd_write_common_headers (http, buf);

  if (http->debug & (DEBUG_CGI|DEBUG_RW))
    fprintf (stderr, "httpd %d cgi read\n", http->c.sockfd);
  while (1) {
    if (hin_find_line (source, &line) == 0) { hin_buffer_clean (buf); return 0; }
    if (line.len == 0) break;
    if (http->debug & (DEBUG_CGI|DEBUG_RW))
      fprintf (stderr, " %zd '%.*s'\n", line.len, (int)line.len, line.ptr);
    if (matchi_string (&line, "Status:") > 0) {
    } else if ((http->peer_flags & HIN_HTTP_CHUNKED) && matchi_string (&line, "Content-Length:") > 0) {
    } else if (HIN_HTTPD_DISABLE_POWERED_BY && matchi_string (&line, "X-Powered-By:") > 0) {
    } else if (matchi_string (&line, "Location:") > 0) {
      hin_cgi_location (http, buf, &line);
    } else if (match_string (&line, "X-CGI-") > 0) {
    } else {
      header (buf, "%.*s\r\n", line.len, line.ptr);
    }
  }
  header (buf, "\r\n");

  if (http->debug & DEBUG_RW) {
    printf ("httpd %d cgi response %d '\n%.*s'\n", http->c.sockfd, buf->count, buf->count, buf->ptr);
    for (basic_dlist_t * elem = buf->list.next; elem; elem=elem->next) {
      hin_buffer_t * buf = hin_buffer_list_ptr (elem);
      printf (" cont %d '\n%.*s'\n", buf->count, buf->count, buf->ptr);
    }
    printf (" left after is %d\n", len);
  }

  hin_pipe_append_raw (pipe, buf);

  if (len > 0) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
    hin_pipe_write_process (pipe, buf1, HIN_PIPE_ALL);
  }

  hin_pipe_start (pipe);

  return -1;
}

static int hin_cgi_headers_close_callback (hin_buffer_t * buffer, int ret) {
  hin_fcgi_worker_t * worker = buffer->parent;
  httpd_client_t * http = worker->http;
  httpd_error (http, 500, "cgi process failed %s", strerror (-ret));
  hin_fcgi_worker_free (worker);
  return 1;
}

static int hin_cgi_headers_eat_callback (hin_buffer_t * buffer, int num) {
  if (num == 0) {
    hin_lines_request (buffer, 0);
    return 0;
  }
  return 1;
}

int hin_cgi_send (httpd_client_t * http, hin_fcgi_worker_t * worker, int fd) {
  hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
  buf->fd = fd;
  buf->parent = worker;
  buf->flags = 0;
  buf->debug = http->debug;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->read_callback = hin_cgi_headers_read_callback;
  lines->eat_callback = hin_cgi_headers_eat_callback;
  lines->close_callback = hin_cgi_headers_close_callback;
  if (hin_request_read (buf) < 0) {
    httpd_error (http, 503, "error! %d", 435776);
    return -1;
  }
  return 0;
}

int hin_fcgi_send (httpd_client_t * http, hin_buffer_t * buf) {
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->read_callback = hin_cgi_headers_read_callback;
  lines->eat_callback = hin_cgi_headers_eat_callback;
  lines->close_callback = hin_cgi_headers_close_callback;
  return 0;
}

#endif


