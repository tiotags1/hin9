
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>

#include "hin.h"
#include "hin_internal.h"
#include "listen.h"
#include "utils.h"
#include "conf.h"

int hin_socket_do_retry () {
  basic_dlist_t * elem = master.server_retry.next;
  while (elem) {
    hin_server_t * server = basic_dlist_ptr (elem, offsetof (hin_client_t, list));
    elem = elem->next;

    basic_dlist_remove (&master.server_retry, elem);

    server->c.sockfd = hin_socket_listen (NULL, NULL, NULL, server);

    if (hin_server_start_accept (server) < 0) {
      hin_weird_error (35347990);
      return -1;
    }
  }

  return 0;
}

static int hin_server_add_retry (hin_server_t * server) {
  basic_dlist_append (&master.server_retry, &server->c.list);
  printf ("socket busy\n");
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
      hin_debug ("fd %d reuse socket '%s'\n", i, buf1);

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
    hin_error ("unknown socket type");
  }
  return ai_family;
}

int hin_socket_listen (const char * addr, const char * port, const char * sock_type, hin_server_t * ptr) {
  struct addrinfo hints;
  struct addrinfo *result = NULL, *rp;
  int s, sockfd;

  if (ptr) {
    result = ptr->rp_base;
  }
  if (result == NULL) {
    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family	= hin_socket_type (sock_type);
    hints.ai_socktype	= SOCK_STREAM;		// We want a TCP socket
    hints.ai_flags	= AI_PASSIVE;		// All interfaces

    int err = getaddrinfo (addr, port, &hints, &result);
    if (err) {
      hin_error ("getaddrinfo: %s\n", gai_strerror (err));
      return -1;
    }
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sockfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sockfd == -1)
      continue;

    #if HIN_SOCKET_REUSEADDR
    int enable = 1;
    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof (int)) < 0)
      hin_perror ("setsockopt (SO_REUSEADDR) failed\n");
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
        if (hin_server_reuse_socket (ptr)) {
          freeaddrinfo (result);
          close (sockfd);
          return ptr->c.sockfd;
        }
        ptr->rp_base = result;
        hin_server_add_retry (ptr);
        return -1;
      }
    }

    close (sockfd);
  }

  if (ptr) {
    ptr->flags &= ~HIN_FLAG_RETRY;
    ptr->rp_base = NULL;
  }

  freeaddrinfo (result);

  if (rp == NULL) {
    hin_error ("could not bind\n");
    return -1;
  }

  int err = listen (sockfd, SOMAXCONN);
  if (err == -1) {
    hin_perror ("listen\n");
    return -1;
  }

  return sockfd;
}

int hin_request_listen (hin_server_t * server, const char * addr, const char * port, const char * sock_type) {
  server->c.sockfd = hin_socket_listen (addr, port, sock_type, server);

  if (hin_server_start_accept (server) < 0) {
    hin_weird_error (2345325);
    return -1;
  }
  master.num_listen++;

  return 0;
}



