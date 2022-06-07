
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "hin.h"
#include "listen.h"

void hin_client_unlink (hin_client_t * client) {
  if (master.debug & DEBUG_SOCKET) printf ("socket %d unlink\n", client->sockfd);
  hin_server_t * server = (hin_server_t*)client->parent;

  if (client->flags & HIN_SSL)
    hin_client_ssl_cleanup (client);
  basic_dlist_remove (&server->client_list, &client->list);
  free (client);
  master.num_client--;

  if (server->client_list.next) return;

  hin_buffer_t * buf = server->accept_buffer;
  if (buf) return ;

  hin_server_unlink (server);
}

void hin_server_clean (hin_server_t * server) {
  hin_client_t * accept_client = (hin_client_t*)server->accept_buffer->parent;
  free (accept_client);
  hin_buffer_clean (server->accept_buffer);

  if (master.debug & DEBUG_SOCKET)
    printf ("server sockfd %d close\n", server->c.sockfd);

  close (server->c.sockfd);
  free (server);
}

int hin_client_addr (char * str, int len, struct sockaddr * ai_addr, socklen_t ai_addrlen) {
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err = getnameinfo (ai_addr, ai_addrlen,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    fprintf (stderr, "getnameinfo: %s\n", gai_strerror (err));
    return -1;
  }
  snprintf (str, len, "%s:%s", hbuf, sbuf);
  return 0;
}


