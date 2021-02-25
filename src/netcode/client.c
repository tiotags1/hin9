
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "hin.h"
#include "http.h"

void hin_client_unlink (hin_client_t * client) {
  if (master.debug & DEBUG_SOCKET) printf ("socket %d unlink\n", client->sockfd);

  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;
  hin_client_list_remove (&bp->active_client, client);

  free (client);
  master.num_client--;
  hin_check_alive ();
}

void hin_server_clean (hin_client_t * server) {
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;
  for (hin_client_t * elem = bp->active_client; elem; elem = elem->next) {
    //hin_client_shutdown (elem);
  }
  free (bp->accept_client);
  hin_buffer_clean (bp->accept_buffer);
  if (master.debug & DEBUG_SOCKET)
    printf ("server sockfd %d close\n", server->sockfd);
  close (server->sockfd);
  free (server);
}

void hin_client_list_remove (hin_client_t ** list, hin_client_t * new) {
  if (*list == new) {
    *list = new->next;
  } else {
    if (new->next)
      new->next->prev = new->prev;
    if (new->prev)
      new->prev->next = new->next;
  }
  new->next = new->prev = NULL;
}

void hin_client_list_add (hin_client_t ** list, hin_client_t * new) {
  new->next = new->prev = NULL;
  if (*list == NULL) {
  } else {
    new->next = *list;
    (*list)->prev = new;
  }
  *list = new;
}

int handle_client (hin_client_t * client) {
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;

  if (bp->ssl_ctx) {
    hin_ssl_accept_init (client);
  }

  if (bp->client_handle) {
    bp->client_handle (client);
  }

  master.num_client++;

  return 0;
}

int hin_client_addr (char * str, int len, struct sockaddr * ai_addr, socklen_t ai_addrlen) {
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err = getnameinfo (ai_addr, ai_addrlen,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    printf ("getnameinfo2 err '%s'\n", gai_strerror (err));
    return -1;
  }
  snprintf (str, len, "%s:%s", hbuf, sbuf);
  return 0;
}


