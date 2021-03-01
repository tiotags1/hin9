
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "hin.h"
#include "utils.h"
#include "conf.h"

int handle_client (hin_client_t * client);

hin_client_t * new_client (hin_client_t * server) {
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;
  int sz = sizeof (hin_client_t) + bp->user_data_size;
  hin_client_t * new = calloc (1, sz);
  new->parent = server;
  new->magic = HIN_CLIENT_MAGIC;
  return new;
}

int hin_server_accept (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)server;

  if (ret < 0) {
    if (master.quit) return 1;
    printf ("failed to accept ? %d '%s'\n", server->sockfd, strerror (-ret));
    switch (-ret) {
    case EBADF:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
      return 0;
    default: break;
    }
    hin_request_accept (buffer, bp->accept_flags);
    return 0;
  }

  client->sockfd = ret;
  if (master.debug & DEBUG_SOCKET) printf ("socket %d accept\n", ret);
  if (handle_client (client) < 0) {
    if (master.quit) return 1;
    hin_request_accept (buffer, bp->accept_flags);
    printf ("client rejected ?\n");
    return 0;
  }

  hin_client_list_add (&bp->active_client, client);

  if (master.quit) return 1;

  hin_client_t * new = new_client (client->parent);
  new->type = client->type;
  new->parent = client->parent;
  buffer->parent = new;

  hin_request_accept (buffer, bp->accept_flags);
  bp->accept_client = new;
  return 0;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

static int hin_socket_type (const char * sock_type) {
  int ai_family = AF_UNSPEC;
  if (sock_type == NULL) {
    ai_family = AF_UNSPEC;
  } else if (strcmp (sock_type, "ipv4") == 0) {
    ai_family = AF_INET;
  } else if (strcmp (sock_type, "ipv6") == 0) {
    ai_family = AF_INET6;
  } else {
    printf ("unknown socket type\n");
  }
  return ai_family;
}

static int create_and_bind (const char * addr, const char *port, const char * sock_type, hin_client_t * client) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sockfd;

  while (1) {

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = hin_socket_type (sock_type);
    hints.ai_socktype = SOCK_STREAM;	// We want a TCP socket
    hints.ai_flags = AI_PASSIVE;	// All interfaces

    s = getaddrinfo (addr, port, &hints, &result);
    if (s != 0) {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
      return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
      sockfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sockfd == -1)
        continue;

      #if HIN_SOCKET_REUSEADDR
      int enable = 1;
      if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof (int)) < 0)
        perror ("setsockopt (SO_REUSEADDR) failed");
      #endif

      s = bind (sockfd, rp->ai_addr, rp->ai_addrlen);
      if (s == 0) {
        // We managed to bind successfully!
        if (client) {
          client->in_addr = *rp->ai_addr;
          client->in_len = rp->ai_addrlen;
        }
        break;
      }

      close (sockfd);
    }

    if (rp == NULL) {
      static int retry_nr = 0;
      if (errno == EADDRINUSE) { printf ("address in use retrying retry %d\n", retry_nr++); sleep (1); continue; }
      freeaddrinfo (result);
      fprintf (stderr, "Could not bind\n");
      return -1;
    }

    freeaddrinfo (result);
    break;
  }

  hin_master_socket_t * sock = &master.share->sockets[master.share->nsocket++];
  sock->sockfd = sockfd;
  sock->in_addr = *rp->ai_addr;
  sock->in_len = rp->ai_addrlen;

  return sockfd;
}

int hin_socket_search (const char * addr, const char *port, const char * sock_type, hin_client_t * client) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sockfd = -1;

  while (1) {
    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = hin_socket_type (sock_type);
    hints.ai_socktype = SOCK_STREAM;	// We want a TCP socket
    hints.ai_flags = AI_PASSIVE;	// All interfaces

    s = getaddrinfo (addr, port, &hints, &result);
    if (s != 0) {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
      return -1;
    }

    hin_master_socket_t * socket = NULL;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      for (int i=0; i < master.share->nsocket; i++) {
        socket = &master.share->sockets[i];
        char buf1[256], buf2[256];
        hin_client_addr (buf1, sizeof buf1, &socket->in_addr, socket->in_len);
        hin_client_addr (buf2, sizeof buf2, rp->ai_addr, rp->ai_addrlen);
        printf ("comparing %d %s %s\n", i, buf1, buf2);
        if (memcmp (&socket->in_addr, rp->ai_addr, rp->ai_addrlen) != 0) { socket = NULL; continue; }
        sockfd = socket->sockfd;
        break;
      }
      if ((sockfd >= 0) && client) {
        client->in_addr = *rp->ai_addr;
        client->in_len = rp->ai_addrlen;
      }
      break;
    }

    freeaddrinfo (result);
    break;
  }

  return sockfd;
}

int hin_socket_listen (const char * address, const char * port, const char * sock_type, hin_client_t * client) {
  int sockfd = create_and_bind (address, port, sock_type, client);
  int err;
  if (sockfd == -1) {
    perror ("create and bind");
    return -1;
  }

  err = listen (sockfd, SOMAXCONN);
  if (err == -1) {
    perror ("listen");
    return -1;
  }

  return sockfd;
}


