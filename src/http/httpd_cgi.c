
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ctype.h>

#include <sys/socket.h>
#include <netdb.h>

#include <hin.h>
#include "http.h"
#include "uri.h"

typedef struct {
  int pos;
  int num;
  char ** env;
} env_list_t;

static int upcase (env_list_t * env) {
  int p = env->pos - 1;
  for (char * ptr=env->env[p]; *ptr && *ptr != '='; ptr++) {
    *ptr = toupper (*ptr);
  }
}

static int var (env_list_t * env, const char * fmt, ...) {
  va_list ap;
  va_start (ap, fmt);

  int p = env->pos;
  env->pos++;
  if (env->pos > env->num) {
    env->num = env->pos + 80;
    env->env = realloc (env->env, env->num * sizeof (char *));
  }
  env->env[env->pos] = NULL;
  int len = vasprintf (&env->env[p], fmt, ap);
  if (master.debug & DEBUG_CGI) fprintf (stderr, "var set %s\n", env->env[p]);

  va_end(ap);
}

static int cgi_done (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("cgi transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  if (pipe->extra_callback) pipe->extra_callback (pipe);
  if (close (pipe->in.fd)) perror ("close in");
  hin_client_t * client = pipe->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  httpd_client_finish_request (client);
}

int hin_pipe_copy_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);

static int cgi_write_headers_callback (hin_buffer_t * buf, int ret) {
  hin_client_t * client = (hin_client_t*)buf->parent;
  httpd_client_t * http = (httpd_client_t*)client->extra;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  pipe->in.fd = (int)buf->data;
  pipe->in.flags = 0;
  pipe->in.pos = 0;
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_DONE | HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->count = 0;
  pipe->ssl = &client->ssl;
  pipe->read_callback = NULL;
  pipe->finish_callback = cgi_done;

  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    pipe->read_callback = hin_pipe_copy_chunked;
  }
  hin_pipe_advance (pipe);
  return 1;
}

static int cgi_read_headers (hin_client_t * client, hin_buffer_t * buffer) {
  httpd_client_t * http = (httpd_client_t*)client->extra;
  string_t source1, * source = &source1;
  source->ptr = buffer->data;
  source->len = buffer->ptr - buffer->data;

  int sz = source->len + 512;
  hin_buffer_t * buf = malloc (sizeof (*buf) + sz);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = cgi_write_headers_callback;
  buf->count = 0;
  buf->sz = sz;
  buf->ptr = buf->buffer;
  buf->parent = client;
  buf->ssl = &client->ssl;
  buf->data = (void*)buffer->fd;

  string_t line, orig=*source, param1, param2;
  int status = 200;
  while (1) {
    if (find_line (source, &line) == 0) { hin_buffer_clean (buf); return 0; }
    if (line.len == 0) break;
    if (master.debug & DEBUG_CGI)
      fprintf (stderr, "cgi header is '%.*s'\n", (int)line.len, line.ptr);
    if (hin_string_equali (&line, "Status: (%d+).*", &param1) > 0) {
      status = atoi (param1.ptr);
      if (master.debug & DEBUG_CGI) printf ("cgi status is %d\n", status);
    } else if (hin_string_equali (&line, "Content%-Encoding: .*") > 0) {
      http->disable |= HIN_HTTP_DEFLATE;
    } else if (hin_string_equali (&line, "Transfer%-Encoding: .*") > 0) {
      http->disable |= HIN_HTTP_CHUNKED;
    } else if (hin_string_equali (&line, "Cache%-Control: .*") > 0) {
      http->disable |= HIN_HTTP_CACHE;
    } else if (hin_string_equali (&line, "Date: .*") > 0) {
      http->disable |= HIN_HTTP_DATE;
    }
  }

  *source = orig;
  header (client, buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, status, http_status_name (status));
  http->peer_flags = http->peer_flags & (~http->disable);
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    header (client, buf, "Content-Encoding: deflate\r\n");
  }
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (client, buf, "Transfer-Encoding: chunked\r\n");
  }
  if ((http->disable & HIN_HTTP_CACHE) == 0) {
    if (http->cache > 0) {
      header (client, buf, "Cache-Control: public, max-age=%ld\r\n", http->cache);
    } else if (http->cache < 0) {
      header (client, buf, "Cache-Control: no-cache, no-store\r\n");
    }
  }
  if ((http->disable & HIN_HTTP_DATE) == 0) {
    time_t rawtime;
    struct tm *info;
    time (&rawtime);
    info = gmtime (&rawtime);
    char buffer[100];
    strftime (buffer, sizeof buffer, "%a, %d %b %Y %X GMT", info);

    header (client, buf, "Date: %s\r\n", buffer);
  }
  while (1) {
    if (find_line (source, &line) == 0) { hin_buffer_clean (buf); return 0; }
    if (line.len == 0) break;
    if (hin_string_equali (&line, "Status: (%d+).*", &param1) > 0) {
    } else {
      header (client, buf, "%.*s\r\n", line.len, line.ptr);
    }
  }
  header (client, buf, "\r\n");

  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    header (client, buf, "%x\r\n%.*s\r\n", source->len, source->len, source->ptr);
  } else {
    header (client, buf, "%.*s", source->len, source->ptr);
  }
  if (master.debug & DEBUG_RW)
    printf ("cgi initial is '%.*s'\n", buf->count, buf->buffer);

  hin_request_write (buf);
  return -1;
}

int cgi_send (hin_client_t * client, int fd) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;

  // TODO disable when the rest is done
  http->disable |= HIN_HTTP_DEFLATE;

  hin_buffer_t * hin_lines_create_cgi (hin_client_t * client, int sockfd, int (*callback) (hin_client_t * client, hin_buffer_t * source));
  hin_buffer_t * buf = hin_lines_create_cgi (client, fd, cgi_read_headers);
}

int httpd_request_chunked (httpd_client_t * http);

int hin_cgi (hin_client_t * client, const char * exe_path, const char * root_path, const char * script_path) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  httpd_request_chunked (http);
  http->state |= HIN_REQ_DATA;

  int out_pipe[2];
  if (pipe (out_pipe) < 0) {
    perror ("pipe");
    return -1;
  }
  int pid = fork ();
  if (pid != 0) {
    // this is root
    if (master.debug & DEBUG_CGI) printf ("cgi pipe fd %d %d pid %d\n", out_pipe[0], out_pipe[1], pid);
    cgi_send (client, out_pipe[0]);
    close (out_pipe[1]);
    return out_pipe[0];
  }
  void httpd_close_socket ();
  httpd_close_socket ();
  if (dup2 (out_pipe[1], STDOUT_FILENO) < 0) {
    perror ("dup2");
    exit (1);
  }
  if (http->post_sz > 0) {
    fprintf (stderr, "cgi stdin set to %d\n", http->post_fd);

    lseek (http->post_fd, 0, SEEK_SET);
    if (dup2 (http->post_fd, STDIN_FILENO) < 0) {
      perror ("dup2");
      exit (1);
    }
  } else {
    if (master.debug & DEBUG_CGI)
    fprintf (stderr, "cgi no stdin\n");
    close (STDIN_FILENO);
  }
  env_list_t env;
  memset (&env, 0, sizeof (env));

  string_t source, line, method, path, param1;
  source = http->headers;
  if (find_line (&source, &line) == 0 || match_string (&line, "(%a+) ("HIN_HTTP_PATH_ACCEPT") HTTP/1.%d", &method, &path) <= 0) {
    fprintf (stderr, "cgi parsing error\n");
    return -1;
  }
  hin_uri_t uri;
  memset (&uri, 0, sizeof (uri));
  if (hin_parse_uri (path.ptr, path.len, &uri) < 0) {
    fprintf (stderr, "error parsing uri\n");
    return -1;
  }
  var (&env, "REQUEST_URI=%.*s", path.len, path.ptr);
  var (&env, "SCRIPT_NAME=%.*s", uri.path.len, uri.path.ptr);
  var (&env, "QUERY_STRING=%.*s", uri.query.len, uri.query.ptr);
  var (&env, "SCRIPT_FILENAME=%s", script_path);
  var (&env, "DOCUMENT_ROOT=%s", root_path);

  var (&env, "REQUEST_METHOD=%.*s", method.len, method.ptr);
  var (&env, "CONTENT_LENGTH=%ld", http->post_sz);
  var (&env, "GATEWAY_INTERFACE=CGI/1.1");
  var (&env, "REDIRECT_STATUS=%d", http->status);
  var (&env, "REQUEST_SCHEME=http");
  var (&env, "SERVER_PROTOCOL=HTTP/1.%d", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1);
  var (&env, "SERVER_SOFTWARE=hin");

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err;
  err = getnameinfo (&client->in_addr, client->in_len,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    var (&env, "REMOTE_ADDR=%s", hbuf);
    var (&env, "REMOTE_PORT=%s", sbuf);
  } else {
    fprintf (stderr, "getnameinfo2 err '%s'\n", gai_strerror (err));
  }
  hin_client_t * server = (hin_client_t*)client->parent;
  err = getnameinfo (&server->in_addr, server->in_len,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    var (&env, "SERVER_ADDR=%s", hbuf);
    var (&env, "SERVER_PORT=%s", sbuf);
  } else {
    fprintf (stderr, "getnameinfo2 err '%s'\n", gai_strerror (err));
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
        var (&env, "CONTENT_TYPE=%.*s", line.len, line.ptr);
      } else {
        var (&env, "HTTP_%.*s=%.*s", param1.len, param1.ptr, line.len, line.ptr);
      }
      upcase (&env);
    }
  }

  if (hostname.ptr) {
    var (&env, "SERVER_NAME=%.*s", hostname.len, hostname.ptr);
  } else {
    var (&env, "SERVER_NAME=unknown");
  }

  char * const argv[] = {(char*)exe_path, (char*)script_path, NULL};
  if (execvpe (argv[0], argv, env.env) < 0) {
    perror ("execv");
  }
  fprintf (stderr, "unexpected\n");
  exit (1);
}


