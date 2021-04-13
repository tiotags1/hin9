
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "hin.h"
#include "utils.h"
#include "conf.h"

static int hin_server_handle_client (hin_client_t * client) {
  hin_server_t * server = (hin_server_t*)client->parent;

  if (server->ssl_ctx) {
    hin_ssl_accept_init (client);
  }

  if (server->client_handle) {
    server->client_handle (client);
  }

  master.num_client++;

  return 0;
}

static hin_client_t * hin_server_new_client (hin_server_t * server) {
  hin_client_t * new = calloc (1, sizeof (hin_client_t) + server->user_data_size);
  new->type = HIN_CLIENT;
  new->magic = HIN_CLIENT_MAGIC;
  new->parent = server;
  server->accept_client = new;
  return new;
}

int hin_server_accept (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_t * bp = (hin_server_t*)server;

  if (ret < 0) {
    if (master.quit) return 1;
    printf ("failed to accept on fd %d '%s'\n", server->sockfd, strerror (-ret));
    switch (-ret) {
    case EBADF:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
      return 0;
    default: break;
    }
    if (hin_request_accept (buffer, bp->accept_flags) < 0) {
      // TODO this should be handled properly
      printf ("accept async failed\n");
      return -1;
    }
    return 0;
  }

  client->sockfd = ret;
  if (master.debug & DEBUG_SOCKET) printf ("socket %d accept\n", ret);
  if (hin_server_handle_client (client) < 0) {
    if (master.quit) return 1;
    if (hin_request_accept (buffer, bp->accept_flags) < 0) {
      printf ("accept sync failed\n");
      return -1;
    }
    printf ("client rejected ?\n");
    return 0;
  }

  hin_client_list_add (&bp->active_client, client);

  if (master.quit) return 1;

  hin_client_t * new = hin_server_new_client (client->parent);
  buffer->parent = new;

  if (hin_request_accept (buffer, bp->accept_flags) < 0) {
    printf ("accept sync failed\n");
    return -1;
  }
  bp->accept_client = new;
  return 0;
}

int hin_server_start_accept (hin_server_t * server) {
  hin_client_t * client = hin_server_new_client (server);

  hin_buffer_t * buffer = calloc (1, sizeof *buffer);
  buffer->fd = server->c.sockfd;
  buffer->parent = client;
  buffer->callback = hin_server_accept;
  server->accept_buffer = buffer;

  if (hin_request_accept (buffer, server->accept_flags) < 0) {
    printf ("conf error\n");
    return -1;
  }

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
          client->ai_addr = *rp->ai_addr;
          client->ai_addrlen = rp->ai_addrlen;
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
  sock->ai_addr = *rp->ai_addr;
  sock->ai_addrlen = rp->ai_addrlen;

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

int hin_socket_request_listen (const char * addr, const char *port, const char * sock_type, hin_server_t * server) {
  struct addrinfo hints, *result, *rp;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family	= hin_socket_type (sock_type);
  hints.ai_socktype	= SOCK_STREAM;		// We want a TCP socket
  hints.ai_flags	= AI_PASSIVE;		// All interfaces

  int s = getaddrinfo (addr, port, &hints, &result);
  if (s != 0) {
    fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
    return -1;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    char buf1[256];
    hin_client_addr (buf1, sizeof buf1, rp->ai_addr, rp->ai_addrlen);
    printf ("socket request '%s'\n", buf1);

    hin_master_socket1_t * socket = calloc (1, sizeof (hin_master_socket1_t));
    socket->sockfd = -1;
    socket->ai_addr = *rp->ai_addr;
    socket->ai_addrlen = rp->ai_addrlen;
    socket->ai_family = rp->ai_family;
    socket->ai_protocol = rp->ai_protocol;
    socket->ai_socktype = rp->ai_socktype;
    socket->server = server;
    if (master.last_socket) {
      master.last_socket->next = socket;
    }
    master.last_socket = socket;
    if (master.socket == NULL) {
      master.socket = socket;
    }
  }

  freeaddrinfo (result);

  return 0;
}

static hin_master_socket_t * hin_socket_search_prev (struct sockaddr * ai_addr, socklen_t ai_addrlen) {
  for (int i = 0; i < master.share->nsocket; i++) {
    hin_master_socket_t * prev = &master.share->sockets[i];
    int len = ai_addrlen > prev->ai_addrlen ? ai_addrlen : prev->ai_addrlen;
    if (memcmp (ai_addr, &prev->ai_addr, len) == 0) {
      return prev;
    }
  }
  return NULL;
}

int hin_socket_do_listen () {
  for (hin_master_socket1_t * sock = master.socket; sock; sock = sock->next) {
    if (sock->server == NULL) continue;
    hin_server_t * server = sock->server;
    if (sock->sockfd >= 0) continue;
    if (server->c.sockfd >= 0) continue;

    char buf1[256];
    hin_client_addr (buf1, sizeof buf1, &sock->ai_addr, sock->ai_addrlen);

    // search in share for the socket
    hin_master_socket_t * new = hin_socket_search_prev (&sock->ai_addr, sock->ai_addrlen);
    if (new) {
      printf ("socket listen '%s' reuse %d\n", buf1, new->sockfd);
      goto just_use;
    }
    printf ("socket listen '%s'\n", buf1);

    int sockfd = socket (sock->ai_family, sock->ai_socktype, sock->ai_protocol);
    if (sockfd == -1) { perror ("socket"); continue; }

    #if HIN_SOCKET_REUSEADDR
    int enable = 1;
    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof (int)) < 0)
      perror ("setsockopt (SO_REUSEADDR) failed");
    #endif

    int err = bind (sockfd, &sock->ai_addr, sock->ai_addrlen);
    if (err == -1) {
      perror ("bind");
      close (sockfd);
      continue;
    }

    err = listen (sockfd, SOMAXCONN);
    if (err == -1) {
      perror ("listen");
      close (sockfd);
      continue;
    }

    sock->sockfd = sockfd;

    new = &master.share->sockets[master.share->nsocket++];
    new->ai_addr = sock->ai_addr;
    new->ai_addrlen = sock->ai_addrlen;
    new->sockfd = sockfd;

just_use:
    server->c.ai_addr = sock->ai_addr;
    server->c.ai_addrlen = sock->ai_addrlen;
    server->c.sockfd = new->sockfd;

    if (hin_server_start_accept (server) < 0) {
      printf ("conf error\n");
      exit (1);
    }
  }

  return 0;
}

int hin_socket_clean () {
  hin_master_socket1_t * next = NULL;
  for (hin_master_socket1_t * sock = master.socket; sock; sock = next) {
    next = sock->next;
    free (sock);
  }
  master.socket = NULL;
  return 0;
}


