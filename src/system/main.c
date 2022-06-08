
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

void hin_clean1 () {
  hin_stop ();
  int hin_log_flush ();
  hin_log_flush ();
  void hin_lua_clean ();
  hin_lua_clean ();
  void hin_console_clean ();
  hin_console_clean ();
  void hin_sharedmem_clean ();
  hin_sharedmem_clean ();
  void hin_cache_clean ();
  hin_cache_clean ();
  int hin_signal_clean ();
  hin_signal_clean ();
  int hin_vfs_clean ();
  hin_vfs_clean ();
  void httpd_vhost_clean ();
  httpd_vhost_clean ();
  #ifdef HIN_USE_FCGI
  void hin_fcgi_clean ();
  hin_fcgi_clean ();
  #endif
  // shouldn't clean pidfile it can incur a race condition
  free ((void*)master.exe_path);
  free ((void*)master.logdir_path);
  free ((void*)master.workdir_path);
  free ((void*)master.tmpdir_path);

  //close (0); close (1); close (2);

  hin_clean ();

  if (master.debug & DEBUG_BASIC)
    printf ("hin close ...\n");
  #ifdef BASIC_USE_MALLOC_DEBUG
  printf ("num fds open %d\n", print_fds ());
  print_unfree ();
  #endif
}

static http_client_t * current_download = NULL;

typedef struct {
  const char * nshort;
  const char * nlong;
  const char * help;
  int cmd;
  int opt;
} hin_arg_t;

enum {CVER=1, CHELP, CPIDFILE, CDAEMONIZE, CPRETEND,
CDIR,
CWORKDIR, CLOGDIR, CTMPDIR, CCONFIG, CDLOG, CREUSE,
CVERBOSE, CQUIET, CLOGLEVEL, CLOGMASK,
CDOWNLOAD, COUTPUT, CPROGRESS, CAUTONAME, CSERVE};

static hin_arg_t cmdlist[] = {
{"-v", "--version", "print version info", CVER, 0},
{"-h", "--help", "print help", CHELP, 0},
{"-d", "--download", "download file and exit", CDOWNLOAD, 0},
{"-o", "--output", "save download to file", COUTPUT, 0},
{"-p", "--progress", "show download progress", CPROGRESS, 0},
{"-n", "--autoname", "derive download name from url", CAUTONAME, 0},
{NULL, "--serve", "start server on <port> without loading any config file", CSERVE, 0},
{"-c", "--config", "load config file at path", CCONFIG, 0},
{NULL, "--log", "set debug log file path", CDLOG, 0},
{NULL, "--pretend", NULL, CPRETEND, 0},
{NULL, "--check", "check config file and exit", CPRETEND, 0},
{NULL, "--pidfile", "set output pidfile", CPIDFILE, 0},
{NULL, "--daemonize", "daemonize service", CDAEMONIZE, 0},
{NULL, "--workdir", "set work dir path", CWORKDIR, CWORKDIR},
{NULL, "--logdir", "set log dir path", CLOGDIR, CLOGDIR},
{NULL, "--tmpdir", "set tmp dir path", CTMPDIR, CTMPDIR},
{NULL, "--reuse", "internal, don't use", CREUSE, 0},
{"-V", "--verbose", "verbose output", CVERBOSE, 0},
{"-q", "--quiet", "print only errors", CQUIET, 0},
{NULL, "--loglevel", "0 prints only errors, 5 prints everything", CLOGLEVEL, 0},
{NULL, "--logmask", "debugmask in hex", CLOGMASK, 0},
{NULL, NULL, NULL, 0, 0},
};

static void print_help () {
  printf ("usage hinsightd [OPTION]...\n");
  for (int i=0; (size_t)i < sizeof (cmdlist) / sizeof (cmdlist[0]) - 1; i++) {
    hin_arg_t * cmd = &cmdlist[i];
    if (cmd->nshort) {
      printf (" %s", cmd->nshort);
    } else {
      printf ("   ");
    }
    printf (" %s", cmd->nlong);
    if (cmd->help) {
      printf ("\t%s", cmd->help);
    }
    printf ("\n");
  }
}

int hin_process_argv (basic_args_t * args, const char * name) {
  hin_arg_t * cmd = NULL;

  for (int i=0; (size_t)i < sizeof (cmdlist) / sizeof (cmdlist[0]); i++) {
    cmd = &cmdlist[i];
    if ((cmd->nshort && strcmp (name, cmd->nshort) == 0) ||
        (cmd->nlong && strcmp (name, cmd->nlong) == 0)) {
      break;
    }
  }

  switch (cmd->cmd) {
  case CVER:
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
    #ifdef HIN_USE_FFCALL
    printf (" ffcall");
    #endif
    printf ("\n");
    return 1;
  break;
  case CHELP:
    print_help ();
    return 1;
  break;
  case CDAEMONIZE:
    master.flags |= HIN_DAEMONIZE;
  break;
  case CPRETEND:
    master.flags |= HIN_PRETEND;
    httpd_vhost_set_debug (0);
  break;
  case CWORKDIR:
  case CLOGDIR:
  case CTMPDIR: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing %s path\n", cmd->nlong);
      print_help ();
      return -1;
    }
    switch (cmd->cmd) {
    case CWORKDIR:
      hin_directory_path (path, &master.workdir_path);
      if (chdir (master.workdir_path) < 0) perror ("chdir");
    break;
    case CLOGDIR:
      hin_directory_path (path, &master.logdir_path);
    break;
    case CTMPDIR:
      hin_directory_path (path, &master.tmpdir_path);
    break;
    }
  break; }
  case CPIDFILE: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing %s path\n", cmd->nlong);
      print_help ();
      return -1;
    }
    if (*path) master.pid_path = path;
  break; }
  case CCONFIG: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing %s path\n", cmd->nlong);
      print_help ();
      return -1;
    }
    master.conf_path = path;
    master.flags &= ~HIN_SKIP_CONFIG;
  break; }
  case CDLOG: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing %s path\n", cmd->nlong);
      print_help ();
      return -1;
    }
    if (hin_redirect_log (path) < 0) return -1;
  break; }
  case CREUSE: {
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
  break; }
  case CVERBOSE:
    httpd_vhost_set_debug (0xffffffff);
  break;
  case CQUIET:
    httpd_vhost_set_debug (0x0);
  break;
  case CLOGLEVEL: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing argument for %s\n", cmd->nlong);
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
    httpd_vhost_set_debug (master.debug);
  break; }
  case CLOGMASK: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing argument for %s\n", cmd->nlong);
      print_help ();
      return -1;
    }
    httpd_vhost_set_debug (strtol (path, NULL, 16));
  break; }
  case CDOWNLOAD: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing argument for %s\n", cmd->nlong);
      print_help ();
      return -1;
    }
    http_client_t * http_download_raw (http_client_t * http, const char * url1);
    http_client_t * http = http_download_raw (NULL, path);
    current_download = http;
    master.flags |= HIN_SKIP_CONFIG;
    master.debug &= ~(DEBUG_BASIC | DEBUG_CONFIG);
    master.flags |= HIN_FLAG_QUIT;
  break; }
  case COUTPUT: {
    const char * path = basic_args_get (args);
    if (path == NULL) {
      printf ("missing argument for %s\n", cmd->nlong);
      print_help ();
      return -1;
    }
    if (current_download == NULL) {
      printf ("no current download for %s\n", cmd->nlong);
      return -1;
    }
    http_client_t * http = current_download;
    http->save_fd = open (path, O_RDWR | O_CLOEXEC | O_TRUNC | O_CREAT, 0666);
    if (http->save_fd < 0) {
      printf ("error! can't open %s %s\n", path, strerror (errno));
      return -1;
    }
  break; }
  case CPROGRESS:
    if (current_download == NULL) {
      printf ("no current download for %s\n", cmd->nlong);
      return -1;
    }
    http_client_t * http = current_download;
    http->debug |= DEBUG_PROGRESS;
  break;
  case CAUTONAME: {
    if (current_download == NULL) {
      printf ("no current download for %s\n", cmd->nlong);
      return -1;
    }
    http_client_t * http = current_download;
    http->flags |= HIN_FLAG_AUTONAME;
  break; }
  case CSERVE: {
    const char * port = basic_args_get (args);
    if (port == NULL) {
      printf ("missing argument for %s\n", cmd->nlong);
      print_help ();
      return -1;
    }
    if (master.debug & DEBUG_CONFIG)
      printf ("serving folder '%s' on port %s\n", ".", port);
    hin_server_t * sock = httpd_create (NULL, port, NULL, NULL);
    httpd_vhost_t * httpd_vhost_get_default ();
    httpd_vhost_t * vhost = httpd_vhost_get_default ();
    sock->c.parent = vhost;
    master.flags |= HIN_SKIP_CONFIG;
  break; }
  default:
    printf ("unkown option '%s'\n", name);
    print_help ();
    return -1;
  break;
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
  hin_directory_path (HIN_TEMP_PATH, &master.tmpdir_path);

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

  int hin_linux_set_limits ();
  hin_linux_set_limits ();
  void hin_init_sharedmem ();
  hin_init_sharedmem ();

  hin_init ();

  void hin_console_init ();
  hin_console_init ();
  int hin_signal_install ();
  hin_signal_install ();

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

  void * hin_cache_create ();
  hin_cache_create ();

  if (master.flags & HIN_FLAG_QUIT) {
  } else if (master.num_listen <= 0) {
    printf ("WARNING! no listen sockets\n");
    return -1;
  } else if (master.debug & DEBUG_BASIC)
    printf ("hin serve ...\n");
  master.share->done = 1;

  while (hin_event_wait ()) {
    hin_event_process ();
  }

  hin_clean1 ();

  return 0;
}


