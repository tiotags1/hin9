
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>

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
  int hin_vfs_clean ();
  hin_vfs_clean ();
  int hin_socket_clean ();
  hin_socket_clean ();
  void hin_vhost_clean ();
  hin_vhost_clean ();
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
 --verbose: print lots of irrelevant information\n\
 --reuse <nr>: used for graceful restart, should never be used otherwise\n\
");
}

static int my_strcmp (const char * base, ...) {
  va_list ap;
  va_start (ap, base);
  const char * ptr = va_arg (ap, const char *);
  int ret = 0;
  while (1) {
    if (strcmp (base, ptr) != 0) { break; }
    ptr = va_arg (ap, const char *);
    if (ptr == NULL) { ret = 1; break; }
  }
  va_end (ap);
  return ret;
}

int hin_process_argv (int argc, const char * argv[]) {
  for (int i = 1; i < argc; i++) {
    if (my_strcmp (argv[i], "--version", NULL)) {
      printf ("%s\n", HIN_HTTPD_SERVER_BANNER);
      return -1;
    } else if (my_strcmp (argv[i], "--help", NULL)) {
      print_help ();
      return -1;
    } else if (my_strcmp (argv[i], "--pidfile", NULL)) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      if (argv[i][0]) {
        master.pid_path = argv[i];
      }
    } else if (my_strcmp (argv[i], "--daemonize", NULL)) {
      master.flags |= HIN_DAEMONIZE;
    } else if (my_strcmp (argv[i], "--pretend", "--check", NULL)) {
      master.flags |= HIN_PRETEND;
    } else if (my_strcmp (argv[i], "--workdir", "--cwd", NULL)) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      hin_directory_path (argv[i], &master.workdir_path);
      if (chdir (master.workdir_path) < 0) perror ("chdir");
    } else if (my_strcmp (argv[i], "--logdir", NULL)) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      hin_directory_path (argv[i], &master.logdir_path);
    } else if (my_strcmp (argv[i], "--tmpdir", NULL)) {
      i++;
      if (i >= argc) {
        printf ("missing path\n");
        print_help ();
        return -1;
      }
      hin_directory_path (argv[i], &master.tmpdir_path);
    } else if (my_strcmp (argv[i], "--config", NULL)) {
      i++;
      if (i >= argc) {
        printf ("missing config path\n");
        print_help ();
        return -1;
      }
      master.conf_path = (char *)argv[i];
    } else if (my_strcmp (argv[i], "--reuse", NULL)) {
      i++;
      if (i >= argc) {
        printf ("missing reuse fd\n");
        print_help ();
        return -1;
      }
      int fd = atoi (argv[i]);
      if (fcntl (fd, F_GETFD) == -1) {
        printf ("fd %d is not opened\n", fd);
        exit (1);
      }
      master.sharefd = fd;
    } else if (my_strcmp (argv[i], "--verbose", NULL)) {
      master.debug = 0xffffffff;
    }
  }
  return 0;
}

int main (int argc, const char * argv[], const char * envp[]) {
  memset (&master, 0, sizeof master);
  master.conf_path = HIN_CONF_PATH;
  master.exe_path = realpath ((char*)argv[0], NULL);
  master.argv = argv;
  master.envp = envp;
  hin_directory_path (HIN_LOGDIR_PATH, &master.logdir_path);
  hin_directory_path (HIN_WORKDIR_PATH, &master.workdir_path);

  master.debug = 0xffffffff;
  master.debug = 0;

  if (hin_process_argv (argc, argv) < 0)
    return -1;

  if (HIN_PRINT_GREETING)
    printf ("hin start ...\n");

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



