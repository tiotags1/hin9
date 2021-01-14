
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <hin.h>

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
  if (master.debug & DEBUG_SOCKET) printf ("connect complete %d %s:%s\n", client->sockfd, hbuf, sbuf);

  int (*callback) (hin_client_t * client, int ret) = NULL;
  memcpy (&callback, buffer->buffer, sizeof (void*));
  callback (client, fd);

  freeaddrinfo ((struct addrinfo *)buffer->data);
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

  int (*callback) (hin_client_t * client, int ret) = NULL;
  memcpy (&callback, buffer->buffer, sizeof (void*));
  callback (client, err);

  printf ("connect failed %d %s:%s '%s'\n", client->sockfd, hbuf, sbuf, strerror (-err));
  freeaddrinfo ((struct addrinfo *)buffer->data);
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
    hin_request_connect (buffer);
    return 0;
  }

  fail (buffer, 0);
  return -1;
}

static int hin_connect_recheck (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;

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

hin_client_t * hin_connect (const char * host, const char * port, int extra_size, int (*callback) (hin_client_t * client, int ret)) {
  if (master.debug & DEBUG_SOCKET) printf ("connect start %s:%s\n", host, port);
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int sfd, s, j;
  size_t len;
  if (callback == NULL) {
    printf ("can't connect without a callback ?\n");
    return NULL;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  s = getaddrinfo (host, port, &hints, &result);
  if (s != 0) {
    fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
    return NULL;
  }

  hin_client_t * client = calloc (1, sizeof *client + extra_size);
  //client->type = SOCKET_CONNECT;
  client->sockfd = -1;
  client->magic = HIN_CONNECT_MAGIC;

  hin_buffer_t * buffer = calloc (1, sizeof *buffer + sizeof (void*));
  buffer->data = buffer->ptr = (char*)result;
  buffer->parent = client;
  memcpy (&buffer->buffer, &callback, sizeof (void*));
  client->read_buffer = buffer;
  hin_connect_try_next (buffer);

  return client;
}





