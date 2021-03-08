
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include <basic_pattern.h>

#include "hin.h"
#include "conf.h"
#include "http.h"

hin_master_t master;

void hin_clean () {
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

  //close (0); close (1); close (2);

  printf ("hin close ...\n");
  #ifdef BASIC_USE_MALLOC_DEBUG
  printf ("num fds open %d\n", print_fds ());
  print_unfree ();
  #endif
}

void print_help () {
  printf ("help info\n");
}

int hin_process_argv (int argc, const char * argv[]) {
  for (int i = 1; i < argc; i++) {
    if (strcmp (argv[i], "--version") == 0) {
      printf ("%s\n", HIN_HTTPD_SERVER_BANNER);
      return -1;
    } else if (strcmp (argv[i], "--help") == 0) {
      print_help ();
      return -1;
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
  master.conf_path = HIN_CONF_PATH;
  if (hin_process_argv (argc, argv) < 0)
    return -1;

  printf ("hin start ...\n");

  master.exe_path = (char*)argv[0];

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
  }

  #if HIN_HTTPD_WORKER_PREFORKED
  int hin_worker_init ();
  hin_worker_init ();
  #endif

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



