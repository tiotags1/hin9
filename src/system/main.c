
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include <basic_pattern.h>
#include <basic_vfs.h>

#include "hin.h"
#include "conf.h"
#include "http.h"

hin_master_t master;

void hin_clean () {
  int hin_log_flush ();
  hin_log_flush ();
  void hin_lua_clean ();
  hin_lua_clean ();
  for (hin_client_t * elem = master.server_list; elem; elem = elem->next) {
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
  int basic_vfs_clean (basic_vfs_t * vfs);
  extern basic_vfs_t * vfs;
  basic_vfs_clean (vfs);
  free (vfs);
  int hin_socket_clean ();
  hin_socket_clean ();
  // shouldn't clean pidfile it can incur a race condition

  //close (0); close (1); close (2);

  printf ("hin close ...\n");
  #ifdef BASIC_USE_MALLOC_DEBUG
  printf ("num fds open %d\n", print_fds ());
  print_unfree ();
  #endif
}

void print_help () {
  printf ("usage hinsightd [OPTION]...\n\
 --version: prints version information\n\
 --config <path>: sets config path\n\
 --pretend: checks config file and exits\n\
 --logdir <path>: sets log dir path\n\
 --cwd <path>: sets current directory\n\
 --pidfile <path>: prints pid to file, used for daemons\n\
 --daemonize: used for daemons\n\
 --reuse <nr>: used for graceful restart, should never be used otherwise\n\
");
}

int hin_process_argv (int argc, const char * argv[]) {
  for (int i = 1; i < argc; i++) {
    if (strcmp (argv[i], "--version") == 0) {
      printf ("%s\n", HIN_HTTPD_SERVER_BANNER);
      return -1;
    } else if (strcmp (argv[i], "--help") == 0) {
      print_help ();
      return -1;
    } else if (strcmp (argv[i], "--pidfile") == 0) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      if (argv[i][0]) {
        master.pid_path = argv[i];
      }
    } else if (strcmp (argv[i], "--daemonize") == 0) {
      master.flags |= HIN_DAEMONIZE;
    } else if (strcmp (argv[i], "--pretend") == 0) {
      master.flags |= HIN_PRETEND;
    } else if (strcmp (argv[i], "--cwd") == 0) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      master.cwd_path = argv[i];
      if (chdir (master.cwd_path) < 0) perror ("chdir");
    } else if (strcmp (argv[i], "--logdir") == 0) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      master.logdir_path = argv[i];
    } else if (strcmp (argv[i], "--config") == 0) {
      i++;
      if (i >= argc) {
        printf ("missing config path\n");
        print_help ();
        return -1;
      }
      master.conf_path = (char *)argv[i];
    } else if (strcmp (argv[i], "--reuse") == 0) {
      i++;
      if (i >= argc) {
        printf ("missing reuse fd\n");
        print_help ();
        return -1;
      }
      master.sharefd = atoi (argv[i]);
    }
  }
  return 0;
}

int main (int argc, const char * argv[]) {
  memset (&master, 0, sizeof master);
  master.conf_path = HIN_CONF_PATH;
  master.exe_path = (char*)argv[0];
  master.logdir_path = "build/";
  master.cwd_path = "./";

  if (hin_process_argv (argc, argv) < 0)
    return -1;

  printf ("hin start ...\n");

  master.debug = 0xffffffff;
  master.debug = 0;
  //master.debug |= DEBUG_SOCKET;
  //master.debug &= ~(DEBUG_URING);
  void hin_init_sharedmem ();
  hin_init_sharedmem ();
  void hin_event_init ();
  hin_event_init ();
  void hin_console_init ();
  #ifndef HIN_LINUX_BUG_5_11_3
  hin_console_init ();
  #endif
  void hin_timer_init ();
  hin_timer_init ();
  int hin_signal_install ();
  hin_signal_install ();

  int lua_init ();
  if (lua_init () < 0) {
    printf ("could not init lua\n");
    return -1;
  }

  int hin_conf_load (const char * path);
  if (master.debug & DEBUG_CONFIG) printf ("conf path '%s'\n", master.conf_path);
  if (hin_conf_load (master.conf_path) < 0) {
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

  printf ("hin serve ...\n");
  master.share->done = 1;

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



