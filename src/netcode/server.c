
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "hin.h"
#include "hin_internal.h"
#include "listen.h"

void hin_client_close (hin_client_t * client) {
  if (master.debug & DEBUG_SOCKET) hin_debug ("socket %d close\n", client->sockfd);
  hin_server_t * server = (hin_server_t*)client->parent;

  if (client->flags & HIN_SSL)
    hin_client_ssl_cleanup (client);
  basic_dlist_remove (&server->client_list, &client->list);
  free (client);
  master.num_client--;

  if (server->client_list.next) return;

  hin_buffer_t * buf = server->accept_buffer;
  if (buf) return ;

  hin_server_close (server);
}

static int hin_server_do_client_callback (hin_client_t * client) {
  hin_server_t * server = (hin_server_t*)client->parent;

  if (server->ssl_ctx) {
    hin_ssl_accept_init (client);
  }

  int ret = 0;
  if (server->accept_callback) {
    ret = server->accept_callback (client);
  }

  server->num_client++;
  master.num_client++;

  return ret;
}

static hin_client_t * hin_server_create_client (hin_server_t * server) {
  hin_client_t * new = calloc (1, sizeof (hin_client_t) + server->user_data_size);
  new->type = HIN_CLIENT;
  new->magic = HIN_CLIENT_MAGIC;
  new->parent = server;
  return new;
}

static int hin_server_accept_callback (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  if (client == NULL) { hin_weird_error (224230987); return 1; }
  hin_server_t * server = (hin_server_t*)client->parent;

  if (ret < 0) {
    if (server->accept_buffer == NULL) return 1;
    hin_error ("failed accept %d '%s'", server->c.sockfd, strerror (-ret));
    switch (-ret) {
    case EBADF:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
      return 0;
    default: break;
    }
    if (hin_request_accept (buffer, server->accept_flags) < 0) {
      hin_weird_error (5364567);
      return -1;
    }
    return 0;
  }

  client->sockfd = ret;
  if (master.debug & DEBUG_SOCKET) {
    char buf1[256];
    hin_client_addr (buf1, sizeof buf1, &client->ai_addr, client->ai_addrlen);
    hin_debug ("socket %d accept '%s' at %lld\n", ret, buf1, (long long)time (NULL));
  }
  if (hin_server_do_client_callback (client) < 0) {
    if (server->accept_buffer == NULL) return 1;
    if (hin_request_accept (buffer, server->accept_flags) < 0) {
      hin_weird_error (3252543);
      return -1;
    }
    hin_weird_error (432526654);
    return 0;
  }

  basic_dlist_append (&server->client_list, &client->list);

  if (server->accept_buffer == NULL) return 1;

  hin_client_t * new = hin_server_create_client (client->parent);
  buffer->parent = new;

  if (hin_request_accept (buffer, server->accept_flags) < 0) {
    hin_weird_error (435332);
    return -1;
  }
  return 0;
}

int hin_server_start_accept (hin_server_t * server) {
  if (server->c.sockfd < 0) {
    if (server->flags & HIN_FLAG_RETRY) { return 0; }
    hin_error ("didn't listen first");
    return -1;
  }

  hin_buffer_t * buffer = calloc (1, sizeof *buffer);
  buffer->fd = server->c.sockfd;
  buffer->parent = hin_server_create_client (server);
  buffer->callback = hin_server_accept_callback;
  buffer->debug = server->debug;
  server->accept_buffer = buffer;

  if (hin_request_accept (buffer, server->accept_flags) < 0) {
    hin_weird_error (35776893);
    return -1;
  }

  basic_dlist_append (&master.server_list, &server->c.list);

  return 0;
}

int hin_server_close (hin_server_t * server) {
  hin_buffer_t * buf = server->accept_buffer;
  if (buf) {
    free (buf->parent);			// free empty client
    buf->parent = NULL;
    hin_buffer_stop_clean (buf);
    server->accept_buffer = NULL;
  }

  if (server->client_list.next) return 0;

  basic_dlist_remove (&master.server_list, &server->c.list);
  master.num_listen--;

  close (server->c.sockfd);
  free (server);

  hin_check_alive ();

  return 0;
}

#include <sys/socket.h>
#include <netdb.h>

int hin_client_addr (char * str, int len, struct sockaddr * ai_addr, socklen_t ai_addrlen) {
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err = getnameinfo (ai_addr, ai_addrlen,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    hin_error ("getnameinfo: %s", gai_strerror (err));
    return -1;
  }
  snprintf (str, len, "%s:%s", hbuf, sbuf);
  return 0;
}



