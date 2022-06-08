
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <liburing.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "hin.h"
#include "hin_internal.h"
#include "conf.h"

#if HIN_URING_REDUCE_SYSCALLS
#define io_uring_submit1(x)
#else
#define io_uring_submit1(x) io_uring_submit(x);
#endif

static struct io_uring ring;

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

static int hin_request_active (hin_buffer_t * buf) {
  if (buf->flags & HIN_ACTIVE) {
    printf ("buf %p already active\n", buf);
    return -1;
  }
  buf->flags |= HIN_ACTIVE;
  return 0;
}

static inline struct io_uring_sqe * hin_request_sqe () {
  struct io_uring_sqe *sqe = NULL;
  if (ring.ring_fd > 0)
   sqe = io_uring_get_sqe (&ring);
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
  if (hin_request_active (buffer)) return -1;

  if (buffer->flags & HIN_SSL) {
    if (buffer->debug & DEBUG_URING) hin_debug (" req %s buf %p cb %p fd %d\n", buffer->flags & HIN_SOCKET ? "send" : "writ", buffer, buffer->callback, buffer->fd);
    hin_ssl_request_write (buffer);
    return 0;
  }

  if (buffer->flags & HIN_EPOLL) {
    if (buffer->flags & (HIN_FILE|HIN_OFFSETS)) {
      buffer->flags |= HIN_SYNC;
    } else {
      if (hin_epoll_request_write (buffer) < 0) return -1;
      return 0;
    }
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
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req %s buf %p cb %p fd %d\n", buffer->flags & HIN_SOCKET ? "send" : "writ", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_read (hin_buffer_t * buffer) {
  if (hin_request_active (buffer)) return -1;

  if (buffer->flags & HIN_SSL) {
    if (buffer->debug & DEBUG_URING) hin_debug (" req %s buf %p cb %p fd %d\n", buffer->flags & HIN_SOCKET ? "recv" : "read", buffer, buffer->callback, buffer->fd);
    hin_ssl_request_read (buffer);
    return 0;
  }

  if (buffer->flags & HIN_EPOLL) {
    if (buffer->flags & (HIN_FILE|HIN_OFFSETS)) {
      buffer->flags |= HIN_SYNC;
    } else {
      if (hin_epoll_request_read (buffer) < 0) return -1;
      return 0;
    }
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
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req %s buf %p cb %p fd %d\n", buffer->flags & HIN_SOCKET ? "recv" : "read", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_accept (hin_buffer_t * buffer, int flags) {
  if (hin_request_active (buffer)) return -1;

  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_t * server = (hin_client_t*)client->parent;
  client->ai_addrlen = sizeof (client->ai_addr);

  if (buffer->flags & HIN_EPOLL) {
    if (hin_epoll_request_read (buffer) < 0) return -1;
    return 0;
  }
  if (buffer->flags & HIN_SYNC) {
    int ret = accept4 (buffer->fd, &client->ai_addr, &client->ai_addrlen, flags);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_accept (sqe, server->sockfd, &client->ai_addr, &client->ai_addrlen, flags);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req acpt buf %p cb %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_connect (hin_buffer_t * buffer, struct sockaddr * ai_addr, int ai_addrlen) {
  if (buffer->flags & HIN_EPOLL) {
    if (hin_epoll_request_read (buffer) < 0) return -1;
    return 0;
  }
  if (buffer->flags & HIN_SYNC) {
    int ret = connect (buffer->fd, ai_addr, ai_addrlen);
    return hin_request_callback (buffer, ret);
  }

  if (hin_request_active (buffer)) return -1;

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_connect (sqe, buffer->fd, ai_addr, ai_addrlen);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req conn buf %p cb %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_close (hin_buffer_t * buffer) {
  if (buffer->flags & (HIN_SYNC | HIN_EPOLL)) {
    int ret = close (buffer->fd);
    return hin_request_callback (buffer, ret);
  }

  if (hin_request_active (buffer)) return -1;

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_close (sqe, buffer->fd);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req clos buf %p cb %p fd %d\n", buffer, buffer->callback, buffer->fd);
  return 0;
}

int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode) {
  if (hin_request_active (buffer)) return -1;

  if (buffer->flags & (HIN_SYNC | HIN_EPOLL)) {
    int ret = openat (dfd, path, flags, mode);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_openat (sqe, dfd, path, flags, mode);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req open buf %p cb %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_timeout (hin_buffer_t * buffer, struct __kernel_timespec * ts, int count, int flags) {
  if (hin_request_active (buffer)) return -1;

  if (buffer->flags & (HIN_SYNC | HIN_EPOLL)) {
    hin_error ("timeout can't be sync/epoll");
    return -1;
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  io_uring_prep_timeout (sqe, ts, count, flags);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req time buf %p cb %p\n", buffer, buffer->callback);
  return 0;
}

int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask) {
  if (hin_request_active (buffer)) return -1;

  if (buffer->flags & (HIN_SYNC | HIN_EPOLL)) {
    int ret = statx (dfd, path, flags, mask, (struct statx *)buffer->ptr);
    return hin_request_callback (buffer, ret);
  }

  struct io_uring_sqe *sqe = hin_request_sqe ();
  if (buffer->count < (int)sizeof (struct statx)) {
    hin_error ("statx insufficient buf");
    return -1;
  }
  io_uring_prep_statx (sqe, dfd, path, flags, mask, (struct statx *)buffer->ptr);
  io_uring_sqe_set_data (sqe, buffer);
  io_uring_submit1 (&ring);

  if (buffer->debug & DEBUG_URING) hin_debug (" req stat buf %p cb %p\n", buffer, buffer->callback);
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
    hin_error ("io_uring_queue_init %s", strerror (-err));
    exit (1);
  }
  #if HIN_URING_DONT_FORK
  err = io_uring_ring_dontfork (&ring);
  if (err < 0) {
    hin_error ("io_uring_ring_dontfork %s", strerror (-err));
    exit (1);
  }
  #endif

  hin_process_sqe_queue ();

  return 1;
}

int hin_event_clean () {
  if (ring.ring_fd > 0)
    io_uring_queue_exit (&ring);
  memset (&ring, 0, sizeof (ring));
  return 0;
}

int hin_event_wait () {
  if ((master.flags & HIN_FLAG_FINISH)) return 0;

  #ifdef HIN_URING_REDUCE_SYSCALLS
  io_uring_submit (&ring);
  #endif

  while (1) {
    struct io_uring_cqe *cqe;
    int err = io_uring_wait_cqe (&ring, &cqe);
    if (err) {
      if (err == -EINTR) continue;
      hin_error ("io_uring_wait_cqe: %s", strerror (-err));
      io_uring_cqe_seen (&ring, cqe);
    }
    break;
  }

  return 1;
}

int hin_event_process () {
  struct io_uring_cqe *cqes[64];
;
  int err;

  hin_check_alive ();

  if (queue) {
    hin_process_sqe_queue ();
  }

  err = io_uring_peek_batch_cqe (&ring, cqes, 64);
  /*if (err == -1) {
    if (err == -EINTR) continue;
    if (err == -EAGAIN) {
      //printf ("received eagain\n");
      break;
    }
    hin_error ("io_uring_peek_cqe: %s", strerror (-err));
    //io_uring_cqe_seen (&ring, cqe);
    continue;
  }*/

  for (int i=0; i<err; i++) {
    struct io_uring_cqe *cqe = cqes[i];

    hin_buffer_t * buffer = (hin_buffer_t *)cqe->user_data;

    uint32_t debug = buffer->debug;
    if (debug & DEBUG_URING) hin_debug ("req begin buf %p cb %p %d\n", buffer, buffer->callback, cqe->res);

    buffer->flags &= ~HIN_ACTIVE;
    err = buffer->callback (buffer, cqe->res);
    if (err) {
      hin_buffer_clean (buffer);
    }
    io_uring_cqe_seen (&ring, cqe);

    if (debug & DEBUG_URING) hin_debug ("req done. buf %p %d\n", buffer, err);
  }

  return 0;
}

