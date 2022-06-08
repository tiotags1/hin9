
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin/hin.h"

hin_server_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx);

int main (int argc, char * argv[]) {
  memset (&master, 0, sizeof master);
  master.debug = 0;

  hin_init ();

  httpd_create (NULL, "8080", NULL, NULL);

  while (hin_event_wait ()) {
    hin_event_process ();
  }

  hin_clean ();
  return 0;
}


