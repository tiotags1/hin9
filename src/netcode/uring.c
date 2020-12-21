
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <liburing.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h> 

#include <hin.h>

#define QUEUE_DEPTH             256

struct io_uring ring;

int hin_event_init () {
  io_uring_queue_init (QUEUE_DEPTH, &ring, 0);
}

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
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request writ buf %p for callback %p\n", buffer, buffer->callback);
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
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request read buf %p for callback %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_accept (hin_buffer_t * buffer, int flags) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;

  client->in_len = sizeof (client->in_addr);

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_accept (sqe, server->sockfd, &client->in_addr, &client->in_len, flags);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request accept buf %p for callback %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_connect (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_connect (sqe, buffer->fd, &client->in_addr, client->in_len);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request connect buf %p for callback %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_close (hin_buffer_t * buffer) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_close (sqe, buffer->fd);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request close buf %p for callback %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_openat (sqe, dfd, path, flags, mode);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request openat buf %p for callback %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  // I dunno
  //if (buffer->count < sizeof (struct statx)) { printf ("need more space inside buffer to statx\n"); return -1; }
  io_uring_prep_statx (sqe, dfd, path, flags, mask, (struct statx *)buffer->ptr);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit (&ring);
  if (master.debug & DEBUG_URING) printf ("request statx buf %p for callback %p\n", buffer, buffer->callback);
  return 0;
}

int hin_event_loop () {
  struct io_uring_cqe *cqe;
  int err;

  while (master.quit == 0 || master.num_active > 0) {
    if (io_uring_wait_cqe (&ring, &cqe) < 0) {
      printf ("error: io_uring_wait_cqe: %s\n", strerror (errno));
      io_uring_cqe_seen (&ring, cqe);
      continue;
    }

    hin_buffer_t * buffer = (hin_buffer_t *)cqe->user_data;
    if (master.debug & DEBUG_URING) printf ("request done buf %p for callback %p\n", buffer, buffer->callback);

    io_uring_cqe_seen (&ring, cqe);
    err = buffer->callback (buffer, cqe->res);
    if (err < 0) {
      if (buffer->error_callback && buffer->error_callback (buffer, cqe->res)) {}
    }
    if (err) {
      if (master.debug & DEBUG_URING) printf ("cleanup buffer %p\n", buffer);
      hin_buffer_clean (buffer);
    }
  }
}



