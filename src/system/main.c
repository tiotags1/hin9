
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>

#include <basic_args.h>
#include <basic_pattern.h>
#include <basic_vfs.h>

#include "hin.h"
#include "conf.h"
#include "http.h"
#include "vhost.h"
#include "system/system.h"

static void print_help () {
  printf ("usage hinsightd [OPTION]...\n\
 -d --download <url>: download file and exit\n\
 -o --output <path>: save download to file\n\
 -p --progress: show download progress\n\
    --serve <port>: start server on <port> without loading any config file\n\
    --config <path>: sets config path\n\
    --tmpdir <path>: sets tmp dir path\n\
    --logdir <path>: sets log dir path\n\
    --workdir <path>: sets current directory\n\
    --check: checks config file and exit\n\
    --pidfile <path>: prints pid to file, used for daemons\n\
    --daemonize: spawn a daemon from this process's zombie\n\
 -q --quiet: print only error messages\n\
 -V --verbose: print lots of information\n\
    --loglevel <nr>: 0 prints only errors, 5 prints everything\n\
    --debugmask 0x<nr>: debugmask in hex\n\
    --reuse <nr>: used for graceful restart, should never be used otherwise\n\
 -v --version: print program version\n\
 -h --help: print this help\n\
");
}

static http_client_t * current_download = NULL;

int hin_process_argv (basic_args_t * args, const char * name) {
  if (basic_args_cmp (name, "-v", "--version", NULL)) {
    printf ("%s", HIN_HTTPD_SERVER_BANNER);
    #ifdef HIN_USE_OPENSSL
    printf (" openssl");
    #endif
    #ifdef HIN_USE_RPROXY
    printf (" rproxy");
    #endif
    #ifdef HIN_USE_FCGI
    printf (" fcgi");
    #endif
    #ifdef HIN_USE_CGI
    printf (" cgi");
    #endif
    printf ("\n");
    return 1;

  } else if (basic_args_cmp (name, "-h", "--help", NULL)) {
    print_help ();
    return 1;

  } else if (basic_args_cmp (name, "--pidfile", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing pid path\n");
      print_help ();
      return -1;
    }
    if (*path) master.pid_path = path;

  } else if (basic_args_cmp (name, "--daemonize", NULL)) {
    master.flags |= HIN_DAEMONIZE;

  } else if (basic_args_cmp (name, "--pretend", "--check", NULL)) {
    master.flags |= HIN_PRETEND;
    hin_vhost_set_debug (0);

  } else if (basic_args_cmp (name, "--workdir", "--cwd", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing workdir path\n");
      print_help ();
      return -1;
    }
    hin_directory_path (path, &master.workdir_path);
    if (chdir (master.workdir_path) < 0) perror ("chdir");

  } else if (basic_args_cmp (name, "--logdir", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing logdir path\n");
      print_help ();
      return -1;
    }
    hin_directory_path (path, &master.logdir_path);

  } else if (basic_args_cmp (name, "--tmpdir", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing tmpdir path\n");
      print_help ();
      return -1;
    }
    hin_directory_path (path, &master.tmpdir_path);

  } else if (basic_args_cmp (name, "--config", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing config path\n");
      print_help ();
      return -1;
    }
    master.conf_path = path;
    master.flags &= ~HIN_SKIP_CONFIG;

  } else if (basic_args_cmp (name, "--log", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing log path\n");
      print_help ();
      return -1;
    }
    if (hin_redirect_log (path) < 0) return -1;

  } else if (basic_args_cmp (name, "--reuse", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("don't use use --reuse\n");
      print_help ();
      return -1;
    }
    int fd = atoi (path);
    if (fcntl (fd, F_GETFD) == -1) {
      printf ("fd %d is not opened\n", fd);
      exit (1);
    }
    master.sharefd = fd;

  } else if (basic_args_cmp (name, "-V", "--verbose", NULL)) {
    hin_vhost_set_debug (0xffffffff);

  } else if (basic_args_cmp (name, "-q", "--quiet", NULL)) {
    hin_vhost_set_debug (0);

  } else if (basic_args_cmp (name, "--loglevel", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing loglevel\n");
      print_help ();
      return -1;
    }
    int nr = atoi (path);
    master.debug = 0;
    switch (nr) {
    case 5: master.debug = 0xffffffff; break;
    case 4: // fall through
    case 3: // fall through
    case 2: master.debug |= DEBUG_HTTP|DEBUG_CGI|DEBUG_PROXY; // fall through
    case 1: master.debug |= DEBUG_CONFIG|DEBUG_SOCKET|DEBUG_RW_ERROR; // fall through
    case 0: master.debug |= DEBUG_BASIC; break;
    default: printf ("unkown loglevel '%s'\n", path); return -1; break;
    }
    hin_vhost_set_debug (master.debug);

  } else if (basic_args_cmp (name, "--debugmask", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing debugmask\n");
      print_help ();
      return -1;
    }
    hin_vhost_set_debug (strtol (path, NULL, 16));

  // download
  } else if (basic_args_cmp (name, "-d", "--download", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing uri\n");
      print_help ();
      return -1;
    }
    http_client_t * http_download_raw (http_client_t * http, const char * url1);
    http_client_t * http = http_download_raw (NULL, path);
    current_download = http;
    master.flags |= HIN_SKIP_CONFIG;
    master.debug &= ~(DEBUG_BASIC | DEBUG_CONFIG);
    master.flags |= HIN_FLAG_QUIT;

  } else if (basic_args_cmp (name, "-o", "--output", NULL)) {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing arg for output path\n");
      print_help ();
      return -1;
    }
    if (current_download == NULL) {
      printf ("no current download for outpath '%s'\n", path);
      return -1;
    }
    http_client_t * http = current_download;
    http->save_fd = open (path, O_RDWR | O_CLOEXEC | O_TRUNC | O_CREAT, 0666);
    if (http->save_fd < 0) {
      perror ("httpd open");
      return -1;
    }

  } else if (basic_args_cmp (name, "-p", "--progress", NULL)) {
    if (current_download == NULL) {
      printf ("no current download\n");
      return -1;
    }
    http_client_t * http = current_download;
    http->debug |= DEBUG_PROGRESS;

  } else if (basic_args_cmp (name, "--serve", NULL)) {
    const char * port = basic_args_get (args);
    if (port == NULL) {
      printf ("missing port for serve\n");
      print_help ();
      return -1;
    }
    if (master.debug & DEBUG_CONFIG)
      printf ("serving folder '%s' on port %s\n", ".", port);
    hin_server_t * sock = httpd_create (NULL, port, NULL, NULL);
    hin_vhost_t * hin_vhost_get_default ();
    hin_vhost_t * vhost = hin_vhost_get_default ();
    sock->c.parent = vhost;
    master.flags |= HIN_SKIP_CONFIG;

  } else {
    printf ("unkown option '%s'\n", name);
    print_help ();
    return -1;
  }
  return 0;
}

int main (int argc, const char * argv[], const char * envp[]) {
  memset (&master, 0, sizeof master);
  master.conf_path = HIN_CONF_PATH;
  master.exe_path = realpath ((char*)argv[0], NULL);
  master.argv = argv;
  master.envp = envp;
  master.debug = HIN_DEBUG_MASK;
  hin_directory_path (HIN_LOGDIR_PATH, &master.logdir_path);
  hin_directory_path (HIN_WORKDIR_PATH, &master.workdir_path);

  int ret = basic_args_process (argc, argv, hin_process_argv);
  if (ret) {
    if (ret > 0) return 0;
    return -1;
  }

  if (master.debug & DEBUG_BASIC)
    printf ("hin start ...\n");

  if (HIN_RESTRICT_ROOT && geteuid () == 0) {
    if (HIN_RESTRICT_ROOT == 1) {
      printf ("WARNING! process started as root\n");
    } else if (HIN_RESTRICT_ROOT == 2) {
      printf ("ERROR! not allowed to run as root\n");
      exit (1);
    }
  }

  hin_init ();

  int lua_init ();
  void hin_lua_report_error ();
  if (lua_init () < 0) {
    hin_lua_report_error ();
    printf ("could not init lua\n");
    return -1;
  }

  int hin_conf_load (const char * path);
  if (hin_conf_load (master.conf_path) < 0) {
    hin_lua_report_error ();
    return -1;
  }

  if (master.flags & HIN_PRETEND) {
    return 0;
  }
  if (master.flags & HIN_DAEMONIZE) {
    int hin_daemonize ();
    if (hin_daemonize () < 0) { return -1; }
  }
  // pid file always goes after daemonize
  if (master.pid_path) {
    int hin_pidfile (const char * path);
    if (hin_pidfile (master.pid_path) < 0) { return -1; }
  }
  if (hin_socket_do_listen () < 0) {
    printf ("error listening to sockets\n");
    return -1;
  }

  if (master.flags & HIN_FLAG_QUIT) {
  } else if (master.num_listen <= 0) {
    printf ("WARNING! no listen sockets\n");
    return -1;
  } else if (master.debug & DEBUG_BASIC)
    printf ("hin serve ...\n");
  master.share->done = 1;

  void * hin_cache_create ();
  hin_cache_create ();

  hin_check_alive ();

  void hin_event_loop ();
  hin_event_loop ();

  void hin_clean ();
  hin_clean ();

  return 0;
}


