
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hin.h"

hin_master_t master;

#include "listen.h"

int hin_init () {
  void hin_linux_set_limits ();
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
  return 0;
}

int hin_clean () {
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

  if (master.debug & DEBUG_BASIC)
    printf ("hin close ...\n");
  #ifdef BASIC_USE_MALLOC_DEBUG
  printf ("num fds open %d\n", print_fds ());
  print_unfree ();
  #endif
  return 0;
}

void hin_stop () {
  master.flags |= HIN_FLAG_QUIT;
  hin_client_t * next = NULL;
  for (hin_client_t * cur = master.server_list; cur; cur = next) {
    if (master.debug & DEBUG_CONFIG)
      printf ("stopping server %d\n", cur->sockfd);
    next = cur->next;
    hin_server_stop ((hin_server_t*)cur);
  }
}

int hin_check_alive () {
  if (master.flags & HIN_RESTARTING) {
    if (master.share->done == 0) {
      printf ("not done\n");
    } else if (master.share->done > 0) {
      printf ("restart succesful\n");
      master.share->done = 0;
      master.flags &= (~HIN_RESTARTING);
      hin_stop ();
    } else {
      printf ("failed to restart\n");
      master.share->done = 0;
    }
  }
  if ((master.flags & HIN_FLAG_QUIT) == 0) return 1;
  if (master.server_list || master.connection_list) {
    if (master.debug & DEBUG_CONFIG) printf ("hin live client %d conn %d\n", master.num_client, master.num_connection);
    return 1;
  }
  master.flags |= HIN_FLAG_FINISH;
  return 0;
}




