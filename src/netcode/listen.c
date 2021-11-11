
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
  master.num_listen--;

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
  struct sockaddr ai_addr;
  socklen_t ai_addrlen;
  for (int i=0; i<1024; i++) {
    ai_addrlen = sizeof (ai_addr);
    int ret = getsockname (i, &ai_addr, &ai_addrlen);
    if (ret) {
      continue;
    }
    if (ai_addrlen != server->c.ai_addrlen) continue;
    if (memcmp (&ai_addr, &server->c.ai_addr, ai_addrlen) != 0) continue;

    char buf1[256];
    int hin_client_addr (char * str, int len, struct sockaddr * ai_addr, socklen_t ai_addrlen);
    hin_client_addr (buf1, sizeof buf1, &ai_addr, sizeof (ai_addr));
    if (server->debug & DEBUG_CONFIG)
      printf ("fd %d reuse socket '%s'\n", i, buf1);

    server->c.sockfd = i;

    return 1;
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

int hin_server_listen (const char * addr, const char * port, const char * sock_type, hin_server_t * ptr) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sockfd;

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
      if (ptr) {
        ptr->c.ai_addr = *rp->ai_addr;
        ptr->c.ai_addrlen = rp->ai_addrlen;
      }
      break;
    } else if (errno == EADDRINUSE) {
      if (ptr) {
        ptr->c.sockfd = sockfd;
        ptr->c.ai_addr = *rp->ai_addr;
        ptr->c.ai_addrlen = rp->ai_addrlen;
        freeaddrinfo (result);
        if (hin_server_reuse_socket (ptr)) {
          close (sockfd);
          return ptr->c.sockfd;
        }
        hin_server_add_retry (ptr);
        return -1;
      }
    }

    close (sockfd);
  }

  if (ptr) {
    ptr->flags &= ~HIN_FLAG_RETRY;
  }

  if (rp == NULL) {
    freeaddrinfo (result);
    fprintf (stderr, "Could not bind\n");
    return -1;
  }

  freeaddrinfo (result);

  err = listen (sockfd, SOMAXCONN);
  if (err == -1) {
    perror ("listen");
    return -1;
  }

  return sockfd;
}

int hin_listen_request (const char * addr, const char * port, const char * sock_type, hin_server_t * server) {
  server->c.sockfd = hin_server_listen (addr, port, sock_type, server);

  if (hin_server_start_accept (server) < 0) {
    printf ("error! %d\n", 2345325);
    return -1;
  }
  master.num_listen++;

  return 0;
}

int hin_listen_do () {
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


