
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include "hin.h"

typedef int (competion_callback_t) (hin_client_t * client, int ret);

static int complete (hin_buffer_t * buffer, int fd) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err = getnameinfo (&client->in_addr, client->in_len,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    printf ("getnameinfo2 err '%s'\n", gai_strerror (err));
  }

  client->sockfd = fd;
  if (master.debug & DEBUG_SOCKET) printf ("connect%s complete %d %s:%s\n", client->flags & HIN_SSL ? "(s)" : "", client->sockfd, hbuf, sbuf);

  competion_callback_t * callback = (competion_callback_t*)buffer->prev;
  int ret = callback (client, fd);

  freeaddrinfo ((struct addrinfo *)buffer->data);
  if (ret) free (client);
  return 1;
}

static int fail (hin_buffer_t * buffer, int err) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err1 = getnameinfo (&client->in_addr, client->in_len,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err1) {
    printf ("getnameinfo3 err '%s'\n", gai_strerror (err));
  }

  printf ("connect%s failed %d %s:%s '%s'\n", client->flags & HIN_SSL ? "(s)" : "", client->sockfd, hbuf, sbuf, strerror (-err));

  competion_callback_t * callback = (competion_callback_t*)buffer->prev;
  int ret = callback (client, err);

  freeaddrinfo ((struct addrinfo *)buffer->data);
  if (ret) free (client);
  return 1;
}

static int hin_connect_recheck (hin_buffer_t * buffer, int ret);

static int hin_connect_try_next (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;

  struct addrinfo * rp = (struct addrinfo*)buffer->ptr;
  for (; rp; rp = rp->ai_next) {
    buffer->fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (buffer->fd < 0) {
      perror ("socket1");
      continue;
    }

    client->in_addr = *rp->ai_addr;
    client->in_len = rp->ai_addrlen;
    buffer->ptr = (char*)rp->ai_next;
    buffer->callback = hin_connect_recheck;
    if (hin_request_connect (buffer) < 0) {
      printf ("connect failed\n");
      return -1;
    }
    return 0;
  }

  fail (buffer, 0);
  return -1;
}

static int hin_connect_recheck (hin_buffer_t * buffer, int ret) {
  if (ret == 0) {
    return complete (buffer, buffer->fd);
  }
  close (buffer->fd);

  if (buffer->ptr) {
    hin_connect_try_next (buffer);
    return 0;
  }

  return fail (buffer, ret);
}

int hin_connect (hin_client_t * client, const char * host, const char * port, int (*callback) (hin_client_t * client, int ret)) {
  if (master.debug & DEBUG_SOCKET) printf ("connect%s start %s:%s\n", client->flags & HIN_SSL ? "(s)" : "", host, port);
  struct addrinfo hints;
  struct addrinfo *result;
  int s;
  if (client == NULL || callback == NULL) {
    fprintf (stderr, "can't connect without a callback ?\n");
    return -1;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  s = getaddrinfo (host, port, &hints, &result);
  if (s != 0) {
    fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
    return -1;
  }

  client->sockfd = -1;
  client->magic = HIN_CONNECT_MAGIC;

  hin_buffer_t * buffer = calloc (1, sizeof *buffer + sizeof (void*));
  buffer->data = buffer->ptr = (char*)result;
  buffer->parent = client;
  buffer->prev = (hin_buffer_t*)callback;
  hin_connect_try_next (buffer);

  return 0;
}





