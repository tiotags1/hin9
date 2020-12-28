
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

void hin_client_shutdown (hin_client_t * client) {
  shutdown (client->sockfd, SHUT_RDWR);
  //shutdown (client->sockfd, SHUT_RD);
  //close (client->sockfd);
  // TODO let client shutdown or else time_wait
  // why does shutdown make it worse ?
}

void hin_client_close (hin_client_t * client) {
  if (master.debug & DEBUG_SOCKET) printf ("socket closed %d\n", client->sockfd);

  //if (close (client->sockfd) < 0) perror ("close socket");

  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
  hin_client_list_remove (&bp->active_client, client);

  free (client);
  master.num_active--;
}

void hin_client_clean (hin_client_t * client) {
  //if (client->read_buffer)
  //  hin_buffer_clean (client->read_buffer);
  //free (client);
  hin_client_shutdown (client);
  close (client->sockfd);
}

void hin_server_clean (hin_client_t * server) {
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
  for (hin_client_t * elem = bp->active_client; elem; elem = elem->next) {
    //hin_client_shutdown (elem);
    hin_client_clean (elem);
  }
  free (bp->accept_client);
  hin_buffer_clean (bp->accept_buffer);
  printf ("closing server sockfd %d\n", server->sockfd);
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
}

void hin_client_list_add (hin_client_t ** list, hin_client_t * new) {
  if (*list == NULL) {
  } else {
    new->next = *list;
    (*list)->prev = new;
  }
  *list = new;
}

int handle_client (hin_client_t * client) {
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;

  if (bp->ssl_ctx) {
    hin_accept_ssl_init (client);
  }

  if (bp->client_handle) {
    bp->client_handle (client);
  }

  master.num_active++;

  return 0;
}


