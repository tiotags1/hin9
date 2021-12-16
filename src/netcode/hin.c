
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hin.h"

hin_master_t master;

#include "listen.h"

int hin_init () {
  int hin_event_init ();
  hin_event_init ();
  int hin_timer_init ();
  hin_timer_init ();
  return 0;
}

int hin_clean () {
  int hin_event_clean ();
  hin_event_clean ();
  return 0;
}

void hin_stop () {
  master.flags |= HIN_FLAG_QUIT;

  basic_dlist_t * elem = master.server_list.next;
  while (elem) {
    hin_server_t * server = basic_dlist_ptr (elem, offsetof (hin_client_t, list));
    elem = elem->next;

    if (master.debug & DEBUG_CONFIG)
      printf ("stopping server %d\n", server->c.sockfd);
    hin_server_stop (server);
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
  if (master.server_list.next || master.num_connection) {
    if (master.debug & DEBUG_CONFIG) printf ("hin live client %d conn %d\n", master.num_client, master.num_connection);
    return 1;
  }
  master.flags |= HIN_FLAG_FINISH;
  return 0;
}




