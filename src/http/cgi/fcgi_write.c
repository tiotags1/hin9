
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <netdb.h>

#include "hin.h"
#include "http.h"
#include "file.h"
#include "vhost.h"
#include "conf.h"

#include <basic_endianness.h>
#include <basic_vfs.h>

#include "fcgi.h"

static int param (hin_buffer_t * buf, const char * name, const char * fmt, ...) {
  va_list ap1, ap2;
  va_start (ap1, fmt);
  va_copy (ap2, ap1);

  char * ptr = NULL;
  int name_len = strlen (name);
  int value_len = vsnprintf (NULL, 0, fmt, ap2);
  int num = 0;

  if (name_len >= 127) {
    printf ("header name '%.*s' too long\n", name_len, name);
    return 0;
  }
  if (value_len >= 127) {
    ptr = header_ptr (buf, 5);
    ptr[0] = name_len;
    ptr[1] = ((value_len >> 24) & 0x7f) | 0x80;
    ptr[2] = ((value_len >> 16) & 0xff);
    ptr[3] = ((value_len >> 8) & 0xff);
    ptr[4] = ((value_len) & 0xff);
    num = 5;
  } else {
    ptr = header_ptr (buf, 2);
    ptr[0] = name_len;
    ptr[1] = value_len;
    num = 2;
  }

  header_raw (buf, name, name_len);
  num += name_len;

  ptr = header_ptr (buf, value_len);
  vsnprintf ((char*)ptr, value_len+1, fmt, ap1);
  num += value_len;

  va_end (ap1);

  if (buf->debug & DEBUG_CGI)
    printf ("param %d '%s' %d '%s'\n", name_len, name, value_len, ptr);
  return num;
}

static int param_header (hin_buffer_t * buf, const char * name, int name_len, const char * value, int value_len) {
  char buffer[128];
  int len = snprintf (buffer, sizeof buffer, "HTTP_%.*s", name_len, name);
  if (len >= sizeof (buffer)) {
    printf ("header name '%.*s' too long\n", name_len, name);
    return 0;
  }
  for (char * ptr = buffer; *ptr && ptr < buffer+sizeof (buffer); ptr++) {
    if (*ptr == '-') *ptr = '_';
    else *ptr = toupper (*ptr);
  }
  return param (buf, buffer, "%.*s", value_len, value);
}

static int hin_fcgi_headers (hin_buffer_t * buf, hin_fcgi_worker_t * worker) {
  httpd_client_t * http = worker->http;
  hin_vhost_t * vhost = (hin_vhost_t*)http->vhost;
  int sz = 0;

  string_t source, line, method, path, param1;
  source = http->headers;
  if (find_line (&source, &line) == 0 || match_string (&line, "(%a+) ("HIN_HTTP_PATH_ACCEPT") HTTP/1.%d", &method, &path) <= 0) {
    fprintf (stderr, "fcgi error! parsing req\n");
    return -1;
  }
  hin_uri_t uri;
  memset (&uri, 0, sizeof (uri));
  if (hin_parse_uri (path.ptr, path.len, &uri) < 0) {
    fprintf (stderr, "fcgi error! parsing uri\n");
    return -1;
  }

  hin_fcgi_socket_t * sock = worker->socket;
  if (http->debug & DEBUG_CGI)
    fprintf (stderr, "fcgi %d http %d\n", sock->fd, http->c.sockfd);

  // if file set then create script path
  extern basic_vfs_t * vfs;
  basic_vfs_node_t * cwd = vhost->cwd_dir;
  basic_vfs_dir_t * cwd_dir = basic_vfs_get_dir (vfs, cwd);
  if (cwd_dir == NULL) { fprintf (stderr, "vfs_get_dir err\n"); return -1; }

  basic_vfs_node_t * file = http->file;
  basic_vfs_dir_t * dir;

  if (file == NULL) {
    fprintf (stderr, "fcgi %d error! can't find path\n", http->c.sockfd);
    return -1;
  }
  dir = file->parent;

/*
  if (path_info && *path_info != '\0') {
    var (&env, "PATH_INFO=%s", path_info);
    char * ptr = (char*)path_info;
    if (*ptr == '/') ptr++;
    var (&env, "PATH_TRANSLATED=%s%s", cwd_dir->path, ptr);
  }*/

  sz += param (buf, "CONTENT_LENGTH", "%d", http->post_sz);
  sz += param (buf, "QUERY_STRING", "%.*s", uri.query.len, uri.query.ptr);

  sz += param (buf, "REQUEST_URI", "%.*s", path.len, path.ptr);

  sz += param (buf, "REDIRECT_STATUS", "%d", http->status);

  int len = cwd_dir->path_len-1;
  sz += param (buf, "SCRIPT_NAME", "%.*s%s", dir->path_len-len, dir->path+len, file->name);

  sz += param (buf, "SCRIPT_FILENAME", "%s%s", dir->path, file->name);
  sz += param (buf, "DOCUMENT_ROOT", "%.*s", cwd_dir->path_len-1, cwd_dir->path);

  sz += param (buf, "REQUEST_METHOD", "%.*s", method.len, method.ptr);
  sz += param (buf, "SERVER_PROTOCOL", "HTTP/1.%d", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1);
  sz += param (buf, "SERVER_SOFTWARE", "%s", HIN_HTTPD_SERVER_BANNER);
  sz += param (buf, "GATEWAY_INTERFACE", "CGI/1.1");
  sz += param (buf, "REQUEST_SCHEME", "http");

  hin_client_t * client = &http->c;
  hin_client_t * socket = (hin_client_t*)client->parent;

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err;
  err = getnameinfo (&socket->ai_addr, socket->ai_addrlen,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    sz += param (buf, "SERVER_PORT", "%s", sbuf);
    sz += param (buf, "SERVER_ADDR", "%s", hbuf);
  } else {
    fprintf (stderr, "getnameinfo2 err '%s'\n", gai_strerror (err));
  }

  err = getnameinfo (&client->ai_addr, client->ai_addrlen,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    sz += param (buf, "REMOTE_ADDR", "%s", hbuf);
    sz += param (buf, "REMOTE_PORT", "%s", sbuf);
  } else {
    fprintf (stderr, "getnameinfo2 err '%s'\n", gai_strerror (err));
  }

  if (client->flags & HIN_SSL) {
    sz += param (buf, "HTTPS", "%d", 1);
  }

  string_t hostname;
  memset (&hostname, 0, sizeof (string_t));
  while (find_line (&source, &line)) {
    if (line.len == 0) break;
    if (match_string (&line, "([%w_%-]+):%s*", &param1) > 0) {
      if (memcmp ("Host:", param1.ptr, 5) == 0) {
        hostname = line;
      }
      if (matchi_string (&param1, "content%-type") > 0) {
        sz += param (buf, "CONTENT_TYPE", "%.*s", line.len, line.ptr);
      } else {
        sz += param_header (buf, param1.ptr, param1.len, line.ptr, line.len);
      }
    }
  }

  sz += param (buf, "SERVER_NAME", "%s", http->hostname ? http->hostname : "unknown");
  return sz;
}

int hin_fcgi_eat_callback (hin_buffer_t * buffer, int num) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;

  if (num > 0) {
    hin_buffer_eat (buffer, num);
    hin_lines_request (buffer, buffer->count);
  } else if (num == 0) {
    hin_lines_request (buffer, buffer->count);
  } else {
    if (lines->close_callback) {
      return lines->close_callback (buffer, num);
    } else {
      printf ("lines client error %d\n", num);
    }
    return -1;
  }
  return 0;
}

static int hin_fcgi_write_callback (hin_buffer_t * buf, int ret) {
  hin_fcgi_worker_t * worker = buf->parent;
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = worker->http;

  if (ret < 0) {
    printf ("fcgi %d write callback error '%s'\n", buf->fd, strerror (-ret));
    hin_fcgi_socket_close (socket);
    return -1;
  }

  if (buf->next) {
    if (hin_request_write (buf->next) < 0) {
      printf ("uring error!\n");
      return -1;
    }
    return 1;
  }

  hin_buffer_t * buf1 = hin_lines_create_raw (READ_SZ);
  buf1->fd = buf->fd;
  buf1->parent = socket;
  buf1->flags = 0;
  buf1->debug = buf->debug;
  hin_lines_t * lines = (hin_lines_t*)&buf1->buffer;
  int hin_fcgi_read_callback (hin_buffer_t * buf, int ret);
  lines->read_callback = hin_fcgi_read_callback;
  lines->eat_callback = hin_fcgi_eat_callback;
  if (hin_request_read (buf1) < 0) {
    httpd_respond_fatal_and_full (http, 503, NULL);
    return -1;
  }
  return 1;
}

int hin_fcgi_write_request (hin_fcgi_worker_t * worker) {
  hin_fcgi_socket_t * socket = worker->socket;
  httpd_client_t * http = worker->http;

  int buf_sz = READ_SZ * 4;
  hin_buffer_t * buf = malloc (sizeof (hin_buffer_t) + buf_sz);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET;
  buf->fd = socket->fd;
  buf->sz = buf_sz;
  buf->ptr = buf->buffer;
  buf->parent = worker;
  buf->debug = http->debug;
  buf->callback = hin_fcgi_write_callback;

  FCGI_Header * head;
  FCGI_BeginRequestBody * body;
  hin_fcgi_header (buf, FCGI_BEGIN_REQUEST, worker->req_id, 8);
  body = header_ptr (buf, sizeof (*body));
  memset (body, 0, sizeof (*body));
  body->role = endian_swap16 (FCGI_RESPONDER);
  body->flags = 0;

  head = hin_fcgi_header (buf, FCGI_PARAMS, worker->req_id, 0);
  int sz = hin_fcgi_headers (buf, worker);
  head->length = endian_swap16 (sz);
  hin_fcgi_header (buf, FCGI_PARAMS, worker->req_id, 0);
  int hin_fcgi_write_post (hin_buffer_t * buf, hin_fcgi_worker_t * worker);
  hin_fcgi_write_post (buf, worker);

  if (hin_request_write (buf) < 0) {
    printf ("uring error!\n");
    return -1;
  }

  int hin_fcgi_pipe_init (hin_fcgi_worker_t * worker);
  hin_fcgi_pipe_init (worker);

  return 0;
}



