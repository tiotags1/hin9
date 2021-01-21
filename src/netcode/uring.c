
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <liburing.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h> 

#include "hin.h"
#include "conf.h"

struct io_uring ring;

int hin_ssl_read (hin_buffer_t * crypt, int ret);
int hin_ssl_write (hin_buffer_t * crypt);

int hin_ssl_handshake (hin_ssl_t * ssl, hin_buffer_t * buf);

int hin_request_write (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_SSL) {
    int extra = 100;
    hin_buffer_t * buf = malloc (sizeof (*buf) + buffer->count + extra);
    memset (buf, 0, sizeof (hin_buffer_t));
    buf->flags = buffer->flags & (~HIN_SSL);
    buf->fd = buffer->fd;
    buf->count = buffer->count + extra;
    buf->sz = buf->count + extra;
    buf->ptr = buf->buffer;
    buf->parent = (void*)buffer;
    buf->ssl = buffer->ssl;
    buf->data = (void*)HIN_SSL_WRITE;
    if (hin_ssl_handshake (buffer->ssl, buf)) { return 0; }
    hin_ssl_write (buf);
    buffer = buf;
  }

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (buffer->flags & HIN_SOCKET) {
    io_uring_prep_send (sqe, buffer->fd, buffer->ptr, buffer->count, 0);
  } else {
    io_uring_prep_write (sqe, buffer->fd, buffer->ptr, buffer->count, buffer->pos);
  }
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d %s buf %p cb %p\n", master.id, buffer->flags & HIN_SOCKET ? "send" : "writ", buffer, buffer->callback);
  return 0;
}

int hin_request_read (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_SSL) {
    hin_buffer_t * buf = malloc (sizeof (*buf) + buffer->count);
    memset (buf, 0, sizeof (hin_buffer_t));
    buf->flags = buffer->flags & (~HIN_SSL);
    buf->fd = buffer->fd;
    buf->count = buffer->count;
    buf->sz = buffer->count;
    buf->ptr = buf->buffer;
    buf->parent = (void*)buffer;
    buf->callback = hin_ssl_read;
    buf->ssl = buffer->ssl;
    buf->data = (void*)HIN_SSL_READ;
    if (hin_ssl_handshake (buffer->ssl, buf)) { return 0; }
    buffer = buf;
  }

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (buffer->flags & HIN_SOCKET) {
    io_uring_prep_recv (sqe, buffer->fd, buffer->ptr, buffer->count, 0);
  } else {
    io_uring_prep_read (sqe, buffer->fd, buffer->ptr, buffer->count, buffer->pos);
  }
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d %s buf %p cb %p\n", master.id, buffer->flags & HIN_SOCKET ? "recv" : "read", buffer, buffer->callback);
  return 0;
}

int hin_request_accept (hin_buffer_t * buffer, int flags) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;

  client->in_len = sizeof (client->in_addr);

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_accept (sqe, server->sockfd, &client->in_addr, &client->in_len, flags);
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d accept buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_connect (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_connect (sqe, buffer->fd, &client->in_addr, client->in_len);
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d connect buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_close (hin_buffer_t * buffer) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_close (sqe, buffer->fd);
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d close buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_openat (sqe, dfd, path, flags, mode);
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d open buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_timeout (hin_buffer_t * buffer, struct timespec * ts, int count, int flags) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_timeout (sqe, ts, count, flags);
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d time buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  // I dunno
  //if (buffer->count < sizeof (struct statx)) { printf ("need more space inside buffer to statx\n"); return -1; }
  io_uring_prep_statx (sqe, dfd, path, flags, mask, (struct statx *)buffer->ptr);
  io_uring_sqe_set_data (sqe, buffer);
  if (master.debug & DEBUG_URING) printf ("req%d stat buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_event_init () {
  struct io_uring_params params;
  memset (&params, 0, sizeof params);

  if (ring.ring_fd > 0)
    io_uring_queue_exit (&ring);

  memset (&ring, 0, sizeof (ring));
  int err = io_uring_queue_init_params (HIN_URING_QUEUE_DEPTH, &ring, &params);
  if (err < 0) {
    fprintf (stderr, "io_uring_queue_init failed %s\n", strerror (-err));
    exit (1);
  }
  #if HIN_URING_DONT_FORK
  err = io_uring_ring_dontfork (&ring);
  if (err < 0) {
    printf ("io_uring_ring_dontfork failed %s\n", strerror (-err));
    exit (1);
  }
  #endif
}

int hin_event_clean () {
  if (ring.ring_fd > 0)
    io_uring_queue_exit (&ring);
  memset (&ring, 0, sizeof (ring));
}

int hin_event_loop () {
  struct io_uring_cqe *cqe;
  int err;

  while (1) {
    io_uring_submit (&ring);
    if ((err = io_uring_wait_cqe (&ring, &cqe)) < 0) {
      if (err == -EINTR) continue;
      printf ("error: io_uring_wait_cqe: %s\n", strerror (errno));
      io_uring_cqe_seen (&ring, cqe);
      continue;
    }

    hin_buffer_t * buffer = (hin_buffer_t *)cqe->user_data;
    if (master.debug & DEBUG_URING) printf ("req%d done buf %p cb %p\n", master.id, buffer, buffer->callback);

    io_uring_cqe_seen (&ring, cqe);
    err = buffer->callback (buffer, cqe->res);
    if (err < 0) {
      if (buffer->error_callback && buffer->error_callback (buffer, cqe->res)) {}
    }
    if (err) {
      if (master.debug & DEBUG_URING) printf ("cleanup%d buffer %p\n", master.id, buffer);
      hin_buffer_clean (buffer);
    }
  }
}



