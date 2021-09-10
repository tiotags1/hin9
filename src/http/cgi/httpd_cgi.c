
#ifdef HIN_USE_CGI

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

int hin_cgi_send (httpd_client_t * http, hin_fcgi_worker_t * worker, int fd);
int httpd_request_chunked (httpd_client_t * http);

int hin_cgi (httpd_client_t * http, const char * exe_path, const char * root_path, const char * script_path, const char * path_info) {
  hin_client_t * client = &http->c;
  hin_client_t * socket = (hin_client_t*)client->parent;
  hin_vhost_t * vhost = (hin_vhost_t*)http->vhost;

  if (http->state & HIN_REQ_DATA) return -1;
  http->state |= (HIN_REQ_DATA | HIN_REQ_CGI);

  if (!HIN_HTTPD_CGI_CHUNKED_UPLOAD && (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD)) {
    httpd_error (http, 411, "cgi spec denies chunked upload");
    return -1;
  }

  if (hin_cache_check (NULL, http) > 0) {
    return 0;
  }

  httpd_request_chunked (http);

  hin_fcgi_worker_t * worker = calloc (1, sizeof (*worker));
  worker->http = http;

  int out_pipe[2];
  if (pipe (out_pipe) < 0) {
    httpd_error (http, 500, "cgi pipe %s", strerror (errno));
    return -1;
  }
  int cwd_fd = dup (vhost->cwd_fd);
  if (cwd_fd < 0) {
    httpd_error (http, 500, "cgi dup %s", strerror (errno));
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
  if (http->method == HIN_METHOD_POST && http->post_fd) {
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
  basic_vfs_node_t * cwd = vhost->cwd_dir;
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

  if (client->flags & HIN_SSL) {
    var (&env, "HTTPS=%d", 1);
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
  } else if (vhost->hostname) {
    var (&env, "SERVER_NAME=%s", vhost->hostname);
  } else {
    var (&env, "SERVER_NAME=unknown");
  }

  char * const argv[] = {(char*)exe_path, (char*)script_path, NULL};
  execvpe (argv[0], argv, env.env);
  perror ("execvpe");
  exit (-1);
}

#endif

