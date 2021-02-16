
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include <basic_pattern.h>

#include "hin.h"
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

  //close (0); close (1); close (2);

  printf ("hin close ...\n");
  #ifdef BASIC_USE_MALLOC_DEBUG
  printf ("num fds open %d\n", print_fds ());
  print_unfree ();
  #endif
}

static void sigint_handler (int signo) {
  printf("^C pressed. Shutting down.\n");
  hin_clean ();
  exit (0);
}

int main (int argc, const char * argv[]) {
  printf ("hin start ...\n");
  signal (SIGINT, sigint_handler);
  //signal (SIGPIPE, SIG_IGN);

  master.exe_path = (char*)argv[0];
  void install_sighandler ();
  install_sighandler ();

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
  #if HIN_HTTPD_WORKER_PREFORKED
  int hin_worker_init ();
  hin_worker_init ();
  #endif
  void hin_init_sharedmem ();
  hin_init_sharedmem ();
  void hin_event_init ();
  hin_event_init ();
  void hin_console_init ();
  hin_console_init ();
  void hin_timer_init ();
  hin_timer_init ();

  int lua_init ();
  if (lua_init () < 0) {
    printf ("could not load config file\n");
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



