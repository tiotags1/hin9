
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <liburing.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h> 

#ifndef STATX_MTIME
#include <linux/stat.h>
#endif

#include "hin.h"
#include "conf.h"

struct io_uring ring;

typedef struct hin_sqe_que_struct {
  struct io_uring_sqe sqe;
  struct hin_sqe_que_struct * next;
} hin_sqe_que_t;

static hin_sqe_que_t * queue = NULL;

int hin_ssl_request_write (hin_buffer_t * buffer);
int hin_ssl_request_read (hin_buffer_t * buffer);
int hin_epoll_request_read (hin_buffer_t * buf);
int hin_epoll_request_write (hin_buffer_t * buf);

static inline int hin_request_callback (hin_buffer_t * buf, int ret) {
  if (ret < 0) ret = -errno;
  int err = buf->callback (buf, ret);
  if (err) {
    hin_buffer_clean (buf);
  }
  return 0;
}

static inline struct io_uring_sqe * hin_request_sqe () {
  struct io_uring_sqe *sqe = io_uring_get_sqe (&ring);
  if (sqe == NULL) {
    hin_sqe_que_t * new = calloc (1, sizeof (*new));
    sqe = &new->sqe;
    new->next = queue;
    queue = new;
  }
  return sqe;
}

static inline void hin_process_sqe_queue () {
  hin_sqe_que_t * new = queue;
  if (new == NULL) return ;
  struct io_uring_sqe * sqe = io_uring_get_sqe (&ring);
  if (sqe == NULL) return ;
  *sqe = new->sqe;
  queue = new->next;
  free (new);
}

int hin_request_is_overloaded () {
  if (queue) { return 1; }
  return 0;
}

int hin_request_write (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_SSL) {
    if (buffer->debug & DEBUG_URING) printf ("req%d %s buf %p cb %p fd %d\n", master.id, buffer->flags & HIN_SOCKET ? "sends" : "writs", buffer, buffer->callback, buffer->fd);
    hin_ssl_request_write (buffer);
    return 0;
  }

  if (buffer->flags & HIN_EPOLL) {
    if (hin_epoll_request_write (buffer) < 0) return -1;
    return 0;
  }
  if (buffer->flags & HIN_SYNC) {
    int ret = write (buffer->fd, buffer->ptr, buffer->count);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
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

  if (buffer->flags & HIN_EPOLL) {
    if (hin_epoll_request_read (buffer) < 0) return -1;
    return 0;
  }
  if (buffer->flags & HIN_SYNC) {
    int ret = read (buffer->fd, buffer->ptr, buffer->count);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();

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
  struct io_uring_sqe *sqe = hin_request_sqe ();
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
  struct io_uring_sqe *sqe = hin_request_sqe ();
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
  client->ai_addrlen = sizeof (client->ai_addr);

  if (buffer->flags & HIN_SYNC) {
    int ret = accept4 (buffer->fd, &client->ai_addr, &client->ai_addrlen, flags);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_accept (sqe, server->sockfd, &client->ai_addr, &client->ai_addrlen, flags);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d accept buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_connect (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;

  if (buffer->flags & HIN_SYNC) {
    int ret = connect (buffer->fd, &client->ai_addr, client->ai_addrlen);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_connect (sqe, buffer->fd, &client->ai_addr, client->ai_addrlen);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d connect buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_close (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_SYNC) {
    int ret = close (buffer->fd);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_close (sqe, buffer->fd);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d close buf %p cb %p fd %d\n", master.id, buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode) {
  if (buffer->flags & HIN_SYNC) {
    int ret = openat (dfd, path, flags, mode);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_openat (sqe, dfd, path, flags, mode);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d open buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_timeout (hin_buffer_t * buffer, struct timespec * ts, int count, int flags) {
  if (buffer->flags & HIN_SYNC) {
    printf ("error timeout can't be sync ?\n");
    return -1;
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_timeout (sqe, ts, count, flags);
  io_uring_sqe_set_data (sqe, buffer);
  if (buffer->debug & DEBUG_URING) printf ("req%d time buf %p cb %p\n", master.id, buffer, buffer->callback);
  return 0;
}

int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask) {
  if (buffer->flags & HIN_SYNC) {
    int ret = statx (dfd, path, flags, mask, (struct statx *)buffer->ptr);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  if (buffer->count < sizeof (struct statx)) { printf ("need more space inside buffer to statx\n"); return -1; }
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
    if (queue) {
      hin_process_sqe_queue ();
    }
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



