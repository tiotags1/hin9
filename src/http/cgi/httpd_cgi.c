
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
#include "worker.h"
#include "conf.h"
#include "hin_lua.h"
#include "file.h"

typedef struct {
  int pos;
  int num;
  uint32_t debug;
  char ** env;
} env_list_t;

static void upcase (env_list_t * env) {
  int p = env->pos - 1;
  for (char * ptr=env->env[p]; *ptr && *ptr != '='; ptr++) {
    if (*ptr == '-') *ptr = '_';
    else {
      *ptr = toupper (*ptr);
    }
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
  char * new = NULL;
  int ret = vasprintf (&new, fmt, ap);
  if (env->debug & DEBUG_CGI) fprintf (stderr, " '%s'\n", new);
  if (ret < 0) { if (new) free (new); return -1; }
  env->env[p] = new;

  va_end (ap);
  return 0;
}

static int hin_pipe_cgi_server_finish_callback (hin_pipe_t * pipe) {
  hin_worker_t * worker = (hin_worker_t *)pipe->parent1;
  httpd_client_t * http = (httpd_client_t*)pipe->parent;
  if (pipe->debug & (DEBUG_CGI|DEBUG_PIPE))
    printf ("pipe %d>%d cgi transfer finished bytes %ld\n", pipe->in.fd, pipe->out.fd, pipe->count);

  if (pipe->debug & (DEBUG_CGI|DEBUG_SYSCALL))
    printf ("  cgi read done, close %d\n", pipe->in.fd);
  close (pipe->in.fd);

  #if HIN_HTTPD_WORKER_PREFORKED
  hin_worker_reset (worker);
  #else
  free (worker);
  #endif

  if (http->c.type == HIN_CACHE_OBJECT) {
    int hin_cache_finish (httpd_client_t * client, hin_pipe_t * pipe);
    return hin_cache_finish (http, pipe);
  } else {
    httpd_client_finish_request (http);
  }
  return 0;
}

int hin_pipe_cgi_server_read_callback (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_worker_t * worker = (hin_worker_t *)pipe->parent1;
  httpd_client_t * http = (httpd_client_t*)worker->data;

  if (num <= 0 || flush) {
    if (http->debug & DEBUG_CGI) printf ("pipe %d>%d cgi subprocess closed\n", pipe->in.fd, pipe->out.fd);
    return 1;
  }

  buffer->count = num;
  hin_pipe_write (pipe, buffer);
  return 0;
}

static int hin_cgi_location (httpd_client_t * http, hin_buffer_t * buf, string_t * origin) {
  string_t source = *origin;
  //if (http->status == 200) http->status = 302; // bad idea ?
  if (match_string (&source, "http") > 0) {
    header (buf, "Location: %.*s\r\n", origin->len, origin->ptr);
    return 0;
  }
  //if (master.debug & DEBUG_CGI)
    //fprintf (stderr, "cgi %d location1 %.*s\n", http->c.sockfd, (int)source.len, source.ptr);
    fprintf (stderr, "cgi %d location2 %.*s\n", http->c.sockfd, (int)origin->len, origin->ptr);
  header (buf, "Location: %.*s\r\n", origin->len, origin->ptr);
  return 0;
}

static int hin_cgi_headers_read_callback (hin_buffer_t * buffer) {
  hin_worker_t * worker = (hin_worker_t *)buffer->parent;
  hin_client_t * client = (hin_client_t*)worker->data;
  httpd_client_t * http = (httpd_client_t*)client;
  string_t source1, * source = &source1;
  source->ptr = buffer->data;
  source->len = buffer->ptr - buffer->data;

  string_t line, orig=*source, param1;
  http->status = 200;
  off_t sz = 0;
  uint32_t disable = 0;
  while (1) {
    if (find_line (source, &line) == 0) { return 0; }
    if (line.len == 0) break;
    if (matchi_string (&line, "Status: (%d+)", &param1) > 0) {
      http->status = atoi (param1.ptr);
      if (http->debug & DEBUG_CGI) printf ("cgi %d status is %d\n", http->c.sockfd, http->status);
    } else if (matchi_string_equal (&line, "Content%-Length: (%d+)", &param1) > 0) {
      sz = atoi (param1.ptr);
    } else if (matchi_string_equal (&line, "Content%-Encoding: .*") > 0) {
      disable |= HIN_HTTP_DEFLATE;
    } else if (matchi_string_equal (&line, "Transfer%-Encoding: .*") > 0) {
      disable |= HIN_HTTP_CHUNKED;
    } else if (match_string (&line, "Cache%-Control:") > 0) {
      disable |= HIN_HTTP_CACHE;
      httpd_parse_cache_str (line.ptr, line.len, &http->cache_flags, &http->cache);
    } else if (matchi_string_equal (&line, "Date: .*") > 0) {
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
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->out.ssl = &client->ssl;
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->parent1 = worker;
  pipe->finish_callback = hin_pipe_cgi_server_finish_callback;
  pipe->debug = http->debug;

  if ((http->cache_flags & HIN_CACHE_PUBLIC) && http->method != HIN_HTTP_POST && http->status == 200) {
    // cache check is somewhere else
    int hin_cache_save (void * store, hin_pipe_t * pipe);
    int n = hin_cache_save (NULL, pipe);
    if (n == 0) {
      // pipe already started ?
      free (pipe);
      return -1;
    } else if (n > 0) {
      if (len > 0) {
        hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
        hin_pipe_append (pipe, buf1);
      }
      hin_pipe_start (pipe);
      return -1;
    }
  }

  http->disable |= disable;

  int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe);
  if (http->method == HIN_HTTP_HEAD) {
    http->peer_flags &= ~(HIN_HTTP_CHUNKED | HIN_HTTP_DEFLATE);
  } else {
    httpd_pipe_set_chunked (http, pipe);
  }

  if ((http->peer_flags & HIN_HTTP_CHUNKED) == 0 && sz > 0) {
    pipe->in.flags |= HIN_COUNT;
    pipe->left = pipe->sz = sz;
  }

  int sz1 = source->len + 512;
  hin_buffer_t * buf = malloc (sizeof (*buf) + sz1);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->count = 0;
  buf->sz = sz1;
  buf->ptr = buf->buffer;
  buf->parent = pipe;
  buf->ssl = &client->ssl;
  buf->debug = http->debug;

  *source = orig;
  header (buf, "HTTP/1.%d %d %s\r\n", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1, http->status, http_status_name (http->status));
  http->peer_flags = http->peer_flags & (~http->disable);

  httpd_write_common_headers (http, buf);

  if (http->debug & (DEBUG_CGI|DEBUG_RW))
    fprintf (stderr, "cgi %d headers\n", http->c.sockfd);
  while (1) {
    if (find_line (source, &line) == 0) { hin_buffer_clean (buf); return 0; }
    if (line.len == 0) break;
    if (http->debug & (DEBUG_CGI|DEBUG_RW))
      fprintf (stderr, " %ld '%.*s'\n", line.len, (int)line.len, line.ptr);
    if (matchi_string (&line, "Status:") > 0) {
    } else if ((http->peer_flags & HIN_HTTP_CHUNKED) && matchi_string (&line, "Content%-Length:") > 0) {
    } else if (HIN_HTTPD_DISABLE_POWERED_BY && matchi_string (&line, "X%-Powered%-By:") > 0) {
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
    for (hin_buffer_t * elem = buf->next; elem; elem=elem->next) {
      printf (" cont %d '\n%.*s'\n", elem->count, elem->count, elem->ptr);
    }
    printf (" left after is %d\n", len);
  }

  hin_pipe_write (pipe, buf);

  if (len > 0) {
    hin_buffer_t * buf1 = hin_buffer_create_from_data (pipe, source->ptr, len);
    hin_pipe_append (pipe, buf1);
  }

  hin_pipe_start (pipe);

  return -1;
}

static int hin_cgi_headers_close_callback (hin_buffer_t * buffer) {
  printf ("httpd cgi process failed\n");
  hin_worker_t * worker = buffer->parent;
  httpd_client_t * http = (httpd_client_t*)worker->data;
  free (worker);
  httpd_respond_fatal (http, 500, NULL);
  return 1;
}

static int hin_cgi_headers_eat_callback (hin_buffer_t * buffer, int num) {
 if (num == 0) {
    hin_lines_request (buffer);
    return 0;
  }
  return 1;
}

int hin_cgi_send (httpd_client_t * http, hin_worker_t * worker, int fd) {
  hin_buffer_t * buf = hin_lines_create_raw ();
  buf->fd = fd;
  buf->parent = (hin_client_t*)worker;
  buf->flags = 0;
  buf->debug = http->debug;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->read_callback = hin_cgi_headers_read_callback;
  lines->eat_callback = hin_cgi_headers_eat_callback;
  lines->close_callback = hin_cgi_headers_close_callback;
  if (hin_request_read (buf) < 0) {
    httpd_respond_fatal_and_full (http, 503, NULL);
    return -1;
  }
  return 0;
}

int httpd_request_chunked (httpd_client_t * http);

int hin_cgi (httpd_client_t * http, const char * exe_path, const char * root_path, const char * script_path, const char * path_info) {
  hin_client_t * client = &http->c;
  hin_client_t * socket = (hin_client_t*)client->parent;
  hin_server_data_t * server = (hin_server_data_t*)socket->parent;

  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= (HIN_REQ_DATA | HIN_REQ_CGI);

  if (!HIN_HTTPD_CGI_CHUNKED_UPLOAD && (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD)) {
    printf ("cgi spec denies chunked upload\n");
    httpd_respond_fatal (http, 411, NULL);
    return -1;
  }

  int hin_cache_check (void * store, httpd_client_t * client);
  if (hin_cache_check (NULL, http) > 0) {
    return 0;
  }

  httpd_request_chunked (http);

  hin_worker_t * worker = calloc (1, sizeof (*worker));
  worker->data = (void*)client;

  int out_pipe[2];
  if (pipe (out_pipe) < 0) {
    perror ("pipe");
    httpd_respond_error (http, 500, NULL);
    return -1;
  }
  int cwd_fd = dup (server->cwd_fd);
  if (cwd_fd < 0) {
    perror ("dup");
    httpd_respond_error (http, 500, NULL);
    return -1;
  }

  int pid = fork ();
  if (pid != 0) {
    // this is root
    if (http->debug & (DEBUG_CGI|DEBUG_SYSCALL)) printf ("cgi %d pipe read fd %d write fd %d pid %d\n", http->c.sockfd, out_pipe[0], out_pipe[1], pid);
    hin_cgi_send (http, worker, out_pipe[0]);
    close (out_pipe[1]);
    close (cwd_fd);
    return out_pipe[0];
  }

  int hin_signal_clean ();
  hin_signal_clean ();

  void httpd_close_socket ();
  httpd_close_socket ();
  int hin_event_clean ();
  hin_event_clean ();

  if (dup2 (out_pipe[1], STDOUT_FILENO) < 0) {
    perror ("dup2");
    exit (1);
  }
  if (http->method == HIN_HTTP_POST && http->post_fd) {
    if (http->debug & DEBUG_CGI)
      fprintf (stderr, "cgi %d stdin set to %d\n", http->c.sockfd, http->post_fd);

    lseek (http->post_fd, 0, SEEK_SET);
    if (dup2 (http->post_fd, STDIN_FILENO) < 0) {
      perror ("dup2");
      exit (1);
    }
  } else {
    if (http->debug & DEBUG_CGI)
      fprintf (stderr, "cgi %d no stdin\n", http->c.sockfd);
    close (STDIN_FILENO);
  }
  env_list_t env;
  memset (&env, 0, sizeof (env));
  env.debug = http->debug;

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

  if (http->debug & DEBUG_CGI) fprintf (stderr, "cgi %d\n", http->c.sockfd);
  int tmp = 0;
  if (root_path) {
    tmp = strlen (root_path);
  } else {
    root_path = ".";
    tmp++;
  }

  exe_path = realpath (exe_path, NULL);

  if (fchdir (cwd_fd)) {
    perror ("fchdir");
    exit (1);
  }
  close (cwd_fd);

  // if file set then create script path
  extern basic_vfs_t * vfs;
  basic_vfs_node_t * cwd = server->cwd_dir;
  basic_vfs_dir_t * cwd_dir = basic_vfs_get_dir (vfs, cwd);
  if (cwd_dir == NULL) { fprintf (stderr, "vfs_get_dir err\n"); exit (1); }

  basic_vfs_node_t * file = http->file;
  basic_vfs_dir_t * dir;

  if (script_path) {
    string_t path;
    path.ptr = (char*)script_path;
    path.len = strlen (path.ptr);
    file = basic_vfs_ref_path (vfs, cwd, &path);
    if (file == NULL) {
      fprintf (stderr, "can't find '%s' in '%s'\n", script_path, cwd_dir->path);
    }
  }

  if (file == NULL) {
    fprintf (stderr, "can't find path\n");
    exit (1);
  }
  dir = file->parent;

  var (&env, "CONTENT_LENGTH=%ld", http->post_sz);
  var (&env, "QUERY_STRING=%.*s", uri.query.len, uri.query.ptr);
  var (&env, "REQUEST_URI=%.*s", path.len, path.ptr);
  if (0) {
    var (&env, "REDIRECT_URI=%.*s", path.len, path.ptr);
  }

  var (&env, "REDIRECT_STATUS=%d", http->status);
  int len = cwd_dir->path_len-1;
  var (&env, "SCRIPT_NAME=%.*s%s", dir->path_len-len, dir->path+len, file->name);

  if (path_info && *path_info != '\0') {
    var (&env, "PATH_INFO=%s", path_info);
    char * ptr = (char*)path_info;
    if (*ptr == '/') ptr++;
    var (&env, "PATH_TRANSLATED=%s%s", cwd_dir->path, ptr);
  }

  var (&env, "SCRIPT_FILENAME=%s%s", dir->path, file->name);
  var (&env, "DOCUMENT_ROOT=%.*s", cwd_dir->path_len-1, cwd_dir->path);

  var (&env, "REQUEST_METHOD=%.*s", method.len, method.ptr);
  var (&env, "SERVER_PROTOCOL=HTTP/1.%d", http->peer_flags & HIN_HTTP_VER0 ? 0 : 1);
  var (&env, "SERVER_SOFTWARE=" HIN_HTTPD_SERVER_BANNER);
  var (&env, "GATEWAY_INTERFACE=CGI/1.1");
  var (&env, "REQUEST_SCHEME=http");

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err;
  err = getnameinfo (&socket->ai_addr, socket->ai_addrlen,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    var (&env, "SERVER_PORT=%s", sbuf);
    var (&env, "SERVER_ADDR=%s", hbuf);
  } else {
    fprintf (stderr, "getnameinfo2 err '%s'\n", gai_strerror (err));
  }

  err = getnameinfo (&client->ai_addr, client->ai_addrlen,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    var (&env, "REMOTE_ADDR=%s", hbuf);
    var (&env, "REMOTE_PORT=%s", sbuf);
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
  } else if (server->hostname) {
    var (&env, "SERVER_NAME=%s", server->hostname);
  } else {
    var (&env, "SERVER_NAME=unknown");
  }

  char * const argv[] = {(char*)exe_path, (char*)script_path, NULL};
  execvpe (argv[0], argv, env.env);
  perror ("execvpe");
  exit (-1);
}


