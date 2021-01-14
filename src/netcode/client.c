
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netdb.h>

#include <hin.h>

static int hin_do_close (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    printf ("error closing %d %s\n", buf->fd, strerror (-ret));
  }
  if (master.debug & DEBUG_SOCKET) printf ("new close %d\n", buf->fd);

  hin_client_t * client = (hin_client_t*)buf->parent;
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
  hin_client_list_remove (&bp->active_client, client);

  free (client);
  master.num_active--;
  return 1;
}

void hin_client_shutdown (hin_client_t * client) {
  if (master.debug & DEBUG_SOCKET) printf ("socket shutdown %d\n", client->sockfd);
  hin_buffer_t * buf = malloc (sizeof *buf);
  memset (buf, 0, sizeof (*buf));
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->fd = client->sockfd;
  buf->callback = hin_do_close;
  buf->parent = client;
  buf->ssl = &client->ssl;
  hin_request_close (buf);
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
  if (master.debug & DEBUG_SOCKET)
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
  new->next = new->prev = NULL;
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


