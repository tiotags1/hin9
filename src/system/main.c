
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

hin_master_t master;

void hin_clean () {
  int hin_log_flush ();
  hin_log_flush ();
  void hin_lua_clean ();
  hin_lua_clean ();
  hin_client_t * next;
  for (hin_client_t * elem = master.server_list; elem; elem = next) {
    next = elem->next;
    void hin_server_clean (hin_client_t * server);
    hin_server_clean (elem);
  }
  void hin_console_clean ();
  hin_console_clean ();
  int hin_event_clean ();
  hin_event_clean ();
  void hin_sharedmem_clean ();
  hin_sharedmem_clean ();
  void hin_cache_clean ();
  hin_cache_clean ();
  int hin_signal_clean ();
  hin_signal_clean ();
  int hin_vfs_clean ();
  hin_vfs_clean ();
  int hin_socket_clean ();
  hin_socket_clean ();
  void hin_vhost_clean ();
  hin_vhost_clean ();
  void hin_fcgi_clean ();
  hin_fcgi_clean ();
  // shouldn't clean pidfile it can incur a race condition
  free ((void*)master.exe_path);
  free ((void*)master.logdir_path);
  free ((void*)master.workdir_path);
  free ((void*)master.tmpdir_path);

  //close (0); close (1); close (2);

  printf ("hin close ...\n");
  #ifdef BASIC_USE_MALLOC_DEBUG
  printf ("num fds open %d\n", print_fds ());
  print_unfree ();
  #endif
}

static void print_help () {
  printf ("usage hinsightd [OPTION]...\n\
 --version: prints version information\n\
 --config <path>: sets config path\n\
 --tmpdir <path>: sets tmp dir path\n\
 --logdir <path>: sets log dir path\n\
 --workdir <path>: sets current directory\n\
 --check: checks config file and exits\n\
 --pidfile <path>: prints pid to file, used for daemons\n\
 --daemonize: used to make daemons\n\
 --quiet: print only error messages\n\
 --verbose: print lots of irrelevant information\n\
 --loglevel <nr>: 0 prints only errors, 5 prints everything\n\
 --debugmask 0x<nr>: debugmask in hex\n\
 --reuse <nr>: used for graceful restart, should never be used otherwise\n\
");
}

int hin_process_argv (basic_args_t * args, const char * name) {
  if (basic_args_cmp (name, "-v", "--version", NULL)) {
    printf ("%s\n", HIN_HTTPD_SERVER_BANNER);
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
    case 0: master.debug |= DEBUG_BASIC;
    case 1: master.debug |= DEBUG_CONFIG|DEBUG_SOCKET|DEBUG_RW_ERROR;
    case 2: master.debug |= DEBUG_HTTP|DEBUG_CGI|DEBUG_PROXY;
    case 3 ... 4:
    case 5: master.debug = 0xffffffff; break;
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

  int hin_linux_set_limits ();
  hin_linux_set_limits ();
  void hin_init_sharedmem ();
  hin_init_sharedmem ();
  void hin_event_init ();
  hin_event_init ();
  void hin_console_init ();
  hin_console_init ();
  void hin_timer_init ();
  hin_timer_init ();
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
  if (master.debug & DEBUG_CONFIG)
    printf ("loading config '%s'\n", master.conf_path);
  if (hin_conf_load (master.conf_path) < 0) {
    hin_lua_report_error ();
    return -1;
  }

  #if HIN_HTTPD_WORKER_PREFORKED
  int hin_worker_init ();
  hin_worker_init ();
  #endif

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

  if (master.debug & DEBUG_BASIC)
    printf ("hin serve ...\n");
  master.share->done = 1;

  void * hin_cache_create ();
  hin_cache_create ();

  //http_download ("http://localhost:28005/cgi-bin/test.php", "/tmp/dl.txt", NULL);
  //http_download ("https://localhost:28006/cgi-bin/test.php", "/tmp/dl.txt", NULL);
  //http_download ("http://localhost:28005/", "/tmp/dl.txt", NULL);
  //http_download ("https://localhost:28006/", "/tmp/dl.txt", NULL);

  void hin_event_loop ();
  hin_event_loop ();

  void hin_clean ();
  hin_clean ();

  return 0;
}



