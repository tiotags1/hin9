
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include "hin.h"

typedef struct {
  struct addrinfo * rp, * base;
  hin_callback_t callback;
  struct sockaddr * ai_addr;
  socklen_t * ai_addrlen;
} hin_connect_t;

void hin_connect_release (int fd) {
  master.num_connection--;
}

static int complete (hin_buffer_t * buf, int ret) {
  hin_connect_t * conn = (hin_connect_t*)buf->buffer;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err = getnameinfo (conn->ai_addr, *conn->ai_addrlen,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
  if (err) {
    hbuf[0] = sbuf[0] = '\0';
    fprintf (stderr, "getnameinfo: %s\n", gai_strerror (err));
  }

  if (ret >= 0) {
    if (master.debug & DEBUG_SOCKET) printf ("connect %d %s:%s complete\n", ret, hbuf, sbuf);
  } else {
    if (master.debug & DEBUG_SOCKET) printf ("connect %s:%s failed! %s\n", hbuf, sbuf, strerror (-ret));
  }

  if (conn->callback (buf, ret)) {
  }
  freeaddrinfo ((struct addrinfo *)conn->base);

  return 1;
}

static int hin_connect_try_next (hin_buffer_t * buf) {
  hin_connect_t * conn = (hin_connect_t*)buf->buffer;

  struct addrinfo * rp = conn->rp;
  for (; rp; rp = rp->ai_next) {
    buf->fd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (buf->fd < 0) {
      perror ("socket");
      continue;
    }

    conn->rp = rp->ai_next;
    *conn->ai_addr = *rp->ai_addr;
    *conn->ai_addrlen = rp->ai_addrlen;
    if (hin_request_connect (buf, conn->ai_addr, *conn->ai_addrlen) < 0) {
      printf ("error! %d\n", 146465465);
      break;
    }
    return 0;
  }

  complete (buf, -ENOSYS);
  return -1;
}

static int hin_connect_recheck (hin_buffer_t * buf, int ret) {
  if (ret == 0) {
    return complete (buf, buf->fd);
  }
  close (buf->fd);

  hin_connect_t * conn = (hin_connect_t*)buf->buffer;
  if (conn->rp) {
    hin_connect_try_next (buf);
    return 0;
  }

  return complete (buf, ret);
}

int hin_connect (const char * host, const char * port, hin_callback_t callback, void * parent, struct sockaddr * ai_addr, socklen_t * ai_addrlen) {
  if (master.debug & DEBUG_SOCKET)
    printf ("connect start %s:%s\n", host, port);
  struct addrinfo hints;
  struct addrinfo *result;
  if (parent == NULL || callback == NULL) {
    fprintf (stderr, "can't connect without a callback ?\n");
    return -1;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  int err = getaddrinfo (host, port, &hints, &result);
  if (err) {
    fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (err));
    return -1;
  }

  hin_buffer_t * buf = calloc (1, sizeof *buf + sizeof (hin_connect_t));
  buf->parent = parent;
  buf->callback = hin_connect_recheck;

  hin_connect_t * conn = (hin_connect_t*)buf->buffer;
  conn->rp = conn->base = result;
  conn->callback = callback;
  conn->ai_addr = ai_addr;
  conn->ai_addrlen = ai_addrlen;
  hin_connect_try_next (buf);

  master.num_connection++;

  return 0;
}

int hin_unix_sock (const char * path, hin_callback_t callback, void * parent) {
  struct sockaddr_un * sock = NULL;
  int len = strlen (path);
  int sz = sizeof (sock->sun_family) + len;

  hin_buffer_t * buf = calloc (1, sizeof *buf + sz);
  buf->callback = callback;
  buf->parent = parent;

  sock = (struct sockaddr_un *)buf->buffer;

  if ((buf->fd = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    return -1;
  }

  sock->sun_family = AF_UNIX;
  memcpy (sock->sun_path, path, len);
  sock->sun_path[len] = '\0';

  if (hin_request_connect (buf, (struct sockaddr *)sock, sz) < 0) {
    printf ("connect unix failed\n");
    return -1;
  }

  return 0;
}




