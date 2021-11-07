
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>

#include "hin.h"
#include "listen.h"
#include "utils.h"
#include "conf.h"

static int hin_server_do_client_callback (hin_client_t * client) {
  hin_server_t * server = (hin_server_t*)client->parent;

  if (server->ssl_ctx) {
    hin_ssl_accept_init (client);
  }

  if (server->accept_callback) {
    server->accept_callback (client);
  }

  server->num_client++;
  master.num_client++;

  return 0;
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
  hin_server_t * server = (hin_server_t*)client->parent;

  if (ret < 0) {
    if (server->accept_buffer == NULL) return 1;
    printf ("error! failed accept %d '%s'\n", server->c.sockfd, strerror (-ret));
    switch (-ret) {
    case EBADF:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
      return 0;
    default: break;
    }
    if (hin_request_accept (buffer, server->accept_flags) < 0) {
      printf ("error! %d\n", 5364567);
      return -1;
    }
    return 0;
  }

  client->sockfd = ret;
  if (master.debug & DEBUG_SOCKET) {
    char buf1[256];
    hin_client_addr (buf1, sizeof buf1, &client->ai_addr, client->ai_addrlen);
    printf ("socket %d accept '%s' at %lld\n", ret, buf1, (long long)time (NULL));
  }
  if (hin_server_do_client_callback (client) < 0) {
    if (server->accept_buffer == NULL) return 1;
    if (hin_request_accept (buffer, server->accept_flags) < 0) {
      printf ("error! %d\n", 3252543);
      return -1;
    }
    printf ("client rejected ?\n");
    return 0;
  }

  hin_client_list_add (&server->client_list, client);

  if (server->accept_buffer == NULL) return 1;

  hin_client_t * new = hin_server_create_client (client->parent);
  buffer->parent = new;

  if (hin_request_accept (buffer, server->accept_flags) < 0) {
    printf ("error! %d\n", 435332);
    return -1;
  }
  return 0;
}

int hin_server_start_accept (hin_server_t * server) {
  if (server->c.sockfd < 0) {
    if (server->flags & HIN_FLAG_RETRY) { return 0; }
    return -1;
  }

  hin_buffer_t * buffer = calloc (1, sizeof *buffer);
  buffer->fd = server->c.sockfd;
  buffer->parent = hin_server_create_client (server);
  buffer->callback = hin_server_accept_callback;
  buffer->debug = server->debug;
  server->accept_buffer = buffer;

  if (hin_request_accept (buffer, server->accept_flags) < 0) {
    printf ("error! %d\n", 43543654);
    return -1;
  }

  hin_client_list_add (&master.server_list, &server->c);

  return 0;
}

int hin_server_unlink (hin_server_t * server) {
  hin_server_stop (server);
  // TODO clean every active client
  close (server->c.sockfd);
  hin_client_list_remove (&master.server_list, &server->c);
  free (server);

  hin_check_alive ();

  return 0;
}

int hin_server_stop (hin_server_t * server) {
  hin_buffer_t * buf = server->accept_buffer;
  if (buf == NULL) return 0;
  free (buf->parent);			// free empty client
  hin_buffer_clean (buf);
  server->accept_buffer = NULL;

  if (server->client_list) return 0;

  hin_server_unlink (server);

  return 0;
}

int hin_server_do_retry () {
  hin_client_t * next = NULL;
  for (hin_client_t * client = master.server_retry; client; client = next) {
    next = client->next;
    hin_server_t * server = (hin_server_t*)client;
    hin_client_list_remove (&master.server_retry, client);

    server->c.sockfd = hin_server_listen (NULL, "8080", NULL, server);

    if (hin_server_start_accept (server) < 0) {
      printf ("error %d\n", 435346);
      return -1;
    }
  }

  return 0;
}

static int hin_server_add_retry (hin_server_t * server) {
  hin_client_list_add ((hin_client_t**)&master.server_retry, &server->c);
  printf ("socket was busy will try again later\n");
  server->flags |= HIN_FLAG_RETRY;
  return 0;
}

static int hin_server_reuse_socket (hin_server_t * server) {
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

    int err = getaddrinfo (addr, port, &hints, &result);
    if (err) {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (err));
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

int hin_server_listen (const char * address, const char * port, const char * sock_type, hin_server_t * client) {
  int sockfd = create_and_bind (address, port, sock_type, client);
  if (sockfd == -1) {
    perror ("create and bind");
    return -1;
  }

  int err = listen (sockfd, SOMAXCONN);
  if (err == -1) {
    perror ("listen");
    return -1;
  }

  return sockfd;
}

int hin_listen_request (const char * addr, const char *port, const char * sock_type, hin_server_t * server) {
  server->c.sockfd = -1;
  server->c.type = HIN_SERVER;
  server->c.magic = HIN_SERVER_MAGIC;

  struct addrinfo hints, *result, *rp;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family	= hin_socket_type (sock_type);
  hints.ai_socktype	= SOCK_STREAM;		// We want a TCP socket
  hints.ai_flags	= AI_PASSIVE;		// All interfaces

  int err = getaddrinfo (addr, port, &hints, &result);
  if (err) {
    fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (err));
    return -1;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if (master.debug & DEBUG_SOCKET) {
      char buf1[256];
      hin_client_addr (buf1, sizeof buf1, rp->ai_addr, rp->ai_addrlen);
      printf ("socket request '%s'\n", buf1);
    }

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

int hin_listen_do () {
  for (hin_master_socket1_t * sock = master.socket; sock; sock = sock->next) {
    if (sock->server == NULL) continue;
    hin_server_t * server = sock->server;
    if (sock->sockfd >= 0) continue;
    if (server->c.sockfd >= 0) continue;

    char buf1[256];

    // search in share for the socket
    hin_master_socket_t * new = hin_socket_search_prev (&sock->ai_addr, sock->ai_addrlen);
    if (master.debug & DEBUG_SOCKET) {
      hin_client_addr (buf1, sizeof buf1, &sock->ai_addr, sock->ai_addrlen);
    }
    if (new) {
      if (master.debug & DEBUG_SOCKET) printf ("socket listen '%s' reuse %d\n", buf1, new->sockfd);
      goto just_use;
    }
    if (master.debug & DEBUG_SOCKET) printf ("socket listen '%s'\n", buf1);

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
      return -1;
    }
    master.num_listen++;
  }

  int err = 0;
  for (hin_master_socket1_t * sock = master.socket; sock; sock = sock->next) {
    if (sock->server == NULL) continue;
    hin_server_t * server = sock->server;
    if (sock->sockfd >= 0) continue;
    if (server->c.sockfd >= 0) continue;

    char buf1[256];
    hin_client_addr (buf1, sizeof buf1, &sock->ai_addr, sock->ai_addrlen);
    printf ("socket couldn't listen to '%s'\n", buf1);
    err = -1;
  }

  return err;
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


