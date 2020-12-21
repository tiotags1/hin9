
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

int handle_client (hin_client_t * client);

hin_client_t * new_client (hin_client_t * server) {
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;
  int sz = sizeof (hin_client_t) + bp->user_data_size;
  hin_client_t * new = calloc (1, sz);
  new->parent = server;
  new->magic = HIN_CLIENT_MAGIC;
  return new;
}

int hin_server_accept (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;

  if (ret < 0) {
    printf ("failed to accept ? %d '%s'\n", server->sockfd, strerror (-ret));
    hin_request_accept (buffer, bp->accept_flags);
    return 0;
  }

  client->sockfd = ret;
  if (master.debug & DEBUG_SOCKET) printf ("socket accept %d\n", ret);
  if (handle_client (client) < 0) {
    hin_request_accept (buffer, bp->accept_flags);
    printf ("client rejected ?\n");
    return 0;
  }

  hin_client_list_add (&bp->active_client, client);
  struct linger sl;
  sl.l_onoff = 1;		// non-zero value enables linger option in kernel
  sl.l_linger = 10;		// timeout interval in seconds
  if (setsockopt (client->sockfd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl)) < 0) 
    perror ("setsockopt(SO_LINGER) failed");

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

int create_and_bind (const char * addr, const char *port, hin_client_t * client) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sockfd;

  while (1) {

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;	// Return IPv4 and IPv6 choices
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

      s = bind (sockfd, rp->ai_addr, rp->ai_addrlen);
      if (s == 0) {
        // We managed to bind successfully!
        if (client) {
          client->in_addr = *rp->ai_addr;
          client->in_len = rp->ai_addrlen;
        }
        #ifndef _WIN32
        struct linger sl;
        sl.l_onoff = 1;		// non-zero value enables linger option in kernel
        sl.l_linger = 0;	// timeout interval in seconds
        if (setsockopt (sockfd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl)) < 0) 
          perror ("setsockopt(SO_LINGER) failed");
        //if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        //  perror ("setsockopt(SO_REUSEADDR) failed");
        //if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0)
        //  perror ("setsockopt(SO_REUSEADDR) failed");
        #endif
        break;
      }

      close (sockfd);
    }

    if (rp == NULL) {
      static int retry_nr = 0;
      if (errno == EADDRINUSE) { printf ("address in use retrying retry %d\n", retry_nr++); sleep (1); continue; }
      fprintf (stderr, "Could not bind\n");
      return -1;
    }

    freeaddrinfo (result);
    break;
  }

  return sockfd;
}

int hin_socket_listen (const char * address, const char * port, hin_client_t * client) {
  int sockfd = create_and_bind (address, port, client);
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


