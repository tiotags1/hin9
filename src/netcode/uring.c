
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

int hin_ssl_request_write (hin_buffer_t * buffer);
int hin_ssl_request_read (hin_buffer_t * buffer);

int hin_request_write (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_SSL) {
    if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, buffer->flags & HIN_SOCKET ? "sends" : "writs", buffer, buffer->callback, buffer->fd);
    hin_ssl_request_write (buffer);
    return 0;
  }

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (buffer->flags & HIN_SOCKET) {
    io_uring_prep_send (sqe, buffer->fd, buffer->ptr, buffer->count, 0);
  } else {
    io_uring_prep_write (sqe, buffer->fd, buffer->ptr, buffer->count, buffer->pos);
  }
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, buffer->flags & HIN_SOCKET ? "send" : "writ", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_read (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_SSL) {
    if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, buffer->flags & HIN_SOCKET ? "recvs" : "reads", buffer, buffer->callback, buffer->fd);
    hin_ssl_request_read (buffer);
    return 0;
  }

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (buffer->flags & HIN_SOCKET) {
    io_uring_prep_recv (sqe, buffer->fd, buffer->ptr, buffer->count, 0);
  } else {
    io_uring_prep_read (sqe, buffer->fd, buffer->ptr, buffer->count, buffer->pos);
  }
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, buffer->flags & HIN_SOCKET ? "recv" : "read", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_write_fixed (hin_buffer_t * buffer) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (buffer->flags & HIN_SOCKET) {
    io_uring_prep_send (sqe, buffer->fd, buffer->ptr, buffer->count, 0);
    //io_uring_prep_send_fixed (sqe, buffer->fd, buffer->ptr, buffer->count, 0, buffer->buf_index);
  } else {
    io_uring_prep_write_fixed (sqe, buffer->fd, buffer->ptr, buffer->count, buffer->pos, buffer->buf_index);
  }
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, "fwrite", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_read_fixed (hin_buffer_t * buffer) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (buffer->flags & HIN_SOCKET) {
    io_uring_prep_recv (sqe, buffer->fd, buffer->ptr, buffer->count, 0);
    //io_uring_prep_recv_fixed (sqe, buffer->fd, buffer->ptr, buffer->count, 0, buffer->buf_index);
  } else {
    io_uring_prep_read_fixed (sqe, buffer->fd, buffer->ptr, buffer->count, buffer->pos, buffer->buf_index);
  }
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, "fread ", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_accept (hin_buffer_t * buffer, int flags) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;
  client->in_len = sizeof (client->in_addr);

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_accept (sqe, server->sockfd, &client->in_addr, &client->in_len, flags);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d accept buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_connect (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;

  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_connect (sqe, buffer->fd, &client->in_addr, client->in_len);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d connect buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_close (hin_buffer_t * buffer) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_close (sqe, buffer->fd);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d close buf %p cb %p fd %d\n", master.id, buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_openat (sqe, dfd, path, flags, mode);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d open buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_timeout (hin_buffer_t * buffer, struct timespec * ts, int count, int flags) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  io_uring_prep_timeout (sqe, ts, count, flags);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d time buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask) {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  // I dunno
  //if (buffer->count < sizeof (struct statx)) { printf ("need more space inside buffer to statx\n"); return -1; }
  io_uring_prep_statx (sqe, dfd, path, flags, mask, (struct statx *)buffer->ptr);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d stat buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

hin_buffer_t * buf_list = NULL;

static int num = 120;
struct iovec iov[120];

int hin_generate_buffers () {
  int sz = 512;
  //memset (iov, 0, sizeof (iov));

  for (int i=0; i<num; i++) {
    hin_buffer_t * buf = malloc (sizeof *buf + sz);
    memset (buf, 0, sizeof (*buf));
    buf->count = buf->sz = sz;
    buf->ptr = buf->buffer;
    buf->buf_index = i;
    iov[i].iov_base = buf->ptr;
    iov[i].iov_len = buf->sz;
    hin_buffer_list_add (&buf_list, buf);
  }

  int ret = io_uring_register_buffers (&ring, iov, num);
  if (ret) {
    fprintf (stderr, "Error registering buffers: %s\n", strerror (-ret));
    return -1;
  }
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
  #if HIN_HTTPD_NULL_SERVER
  hin_generate_buffers ();
  #endif
  return 1;
}

int hin_event_clean () {
  if (ring.ring_fd > 0)
    io_uring_queue_exit (&ring);
  memset (&ring, 0, sizeof (ring));
  hin_buffer_t * next = NULL;
  for (hin_buffer_t * buf = buf_list; buf; buf=next) {
    next = buf->next;
    hin_buffer_clean (buf);
  }
  buf_list = NULL;
  return 0;
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
    if (buffer->debug & DEBUG_URING) printf ("req%d done buf %p cb %p\n", master.id, buffer, buffer->callback);

    io_uring_cqe_seen (&ring, cqe);
    err = buffer->callback (buffer, cqe->res);
    if (err) {
      hin_buffer_clean (buffer);
    }
  }
  return 0;
}



