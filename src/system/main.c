
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

int main (int argc, const char * argv[]) {
  printf ("hin start ...\n");

  master.exe_path = (char*)argv[0];

  for (int i = 0; i<argc; i++) {
    string_t line, param;
    line.ptr = (char*)argv[i];
    line.len = strlen (line.ptr);
    if (match_string (&line, "%-%-reuse=(%d+)", &param) > 0) {
      master.sharefd = atoi (param.ptr);
    }
  }
  master.debug = 0xffffffff;
  master.debug = 0;
  //master.debug |= DEBUG_SOCKET;
  //master.debug &= ~(DEBUG_URING);
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
    printf ("could not load config file\n");
    return -1;
  }

  int hin_conf_load (const char * path);
  int err, loaded = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp (argv[i], "--version") == 0) {
      printf ("%s\n", HIN_HTTPD_SERVER_BANNER);
      exit (0);
    } else if (hin_conf_load (argv[i]) >= 0) {
      loaded = 1;
    }
  }

  if (loaded == 0 && hin_conf_load (HIN_CONF_PATH) < 0) {
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



