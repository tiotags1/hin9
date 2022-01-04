
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zlib.h>

#include "hin.h"

#define USE_LINES 0

int hin_pipe_read_callback (hin_buffer_t * buffer, int ret);
int hin_pipe_write_callback (hin_buffer_t * buffer, int ret);
int hin_pipe_copy_raw (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
static int hin_pipe_process_buffer (hin_pipe_t * pipe, hin_buffer_t * buffer);

static void hin_pipe_close (hin_pipe_t * pipe) {
  pipe->in.flags |= HIN_DONE|HIN_INACTIVE;
  pipe->out.flags |= HIN_DONE;

  pipe->out_error_callback = NULL;
  pipe->in_error_callback = NULL;
  pipe->in.fd = -1;
  pipe->out.fd = -1;

  basic_dlist_t * elem = pipe->write_que.next;
  while (elem) {
    hin_buffer_t * buf = hin_buffer_list_ptr (elem);
    elem = elem->next;

    basic_dlist_remove (&pipe->write_que, &buf->list);
    hin_buffer_clean (buf);
  }

  if (pipe->finish_callback) pipe->finish_callback (pipe);
  pipe->finish_callback = NULL;

  if (pipe->extra) free (pipe->extra);
  pipe->extra = NULL;

  if (pipe->reading.next || pipe->writing.next) return ;

  free (pipe);
}

int hin_pipe_init (hin_pipe_t * pipe) {
  if (pipe->read_callback == NULL)
    pipe->read_callback = hin_pipe_copy_raw;
  if (pipe->buffer_callback == NULL)
    pipe->buffer_callback = hin_pipe_get_buffer;
  return 0;
}

int hin_pipe_start (hin_pipe_t * pipe) {
  if (pipe->flags & HIN_HASH) {
    pipe->hash = 6883;
  }
  pipe->out.flags |= HIN_DONE;
  hin_pipe_advance (pipe);
  return 0;
}

hin_buffer_t * hin_pipe_get_buffer (hin_pipe_t * pipe, int sz) {
#if USE_LINES
  hin_buffer_t * buf = hin_lines_create_raw (sz);
#else
  hin_buffer_t * buf = malloc (sizeof *buf + sz);
  memset (buf, 0, sizeof (*buf));
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = buf->sz = sz;
#endif
  buf->parent = (void*)pipe;
  buf->debug = pipe->debug;
  return buf;
}

int hin_pipe_append_raw (hin_pipe_t * pipe, hin_buffer_t * buf) {
  basic_dlist_append (&pipe->write_que, &buf->list);
  return 0;
}

int hin_pipe_prepend_raw (hin_pipe_t * pipe, hin_buffer_t * buf) {
  basic_dlist_prepend (&pipe->write_que, &buf->list);
  return 0;
}

int hin_pipe_write_process (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  basic_dlist_t * elem = &buffer->list;
  while (elem) {
    hin_buffer_t * buf = hin_buffer_list_ptr (elem);
    elem = elem->next;

    hin_pipe_process_buffer (pipe, buf);
  }
  return 0;
}

static int hin_pipe_process_buffer (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  if (pipe->in.flags & HIN_OFFSETS) {
    pipe->in.pos += buffer->count;
  }

  if (pipe->in.flags & HIN_COUNT) {
    pipe->left -= buffer->count;
    if (pipe->left <= 0) {
      if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d read finished\n", pipe->in.fd, pipe->out.fd);
      pipe->in.flags |= HIN_DONE;
    }
  }

  int ret1 = 0, flush = (pipe->in.flags & HIN_DONE) ? 1 : 0;

  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d append %d bytes %s\n", pipe->in.fd, pipe->out.fd, buffer->count, flush ? "flush" : "cont");

  if (pipe->decode_callback) {
    ret1 = pipe->decode_callback (pipe, buffer, buffer->count, flush);
  } else if (pipe->in_callback) {
    ret1 = pipe->in_callback (pipe, buffer, buffer->count, flush);
  } else {
    ret1 = pipe->read_callback (pipe, buffer, buffer->count, flush);
  }
  if (ret1) hin_buffer_clean (buffer);
  return 0;
}

int hin_pipe_finish (hin_pipe_t * pipe) {
  pipe->in.flags |= HIN_DONE;
  hin_buffer_t * buf = hin_buffer_create_from_data (pipe, NULL, 0);

  hin_pipe_process_buffer (pipe, buf);

  if (pipe->writing.next == NULL) {
    pipe->out.flags |= HIN_DONE;
  }
  hin_pipe_advance (pipe);

  return 0;
}

int hin_pipe_copy_raw (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  if (num <= 0) return 1;
  buffer->count = num;
  hin_pipe_append_raw (pipe, buffer);
  //if (flush) return 1; // already cleaned in the write done handler
  return 0;
}

static int hin_pipe_read_next (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  if (pipe->in.flags & HIN_INACTIVE) return 0;

  buffer->fd = pipe->in.fd;
  buffer->ssl = pipe->in.ssl;
  buffer->flags = pipe->in.flags;
  buffer->parent = pipe;

#if USE_LINES
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  lines->read_callback = hin_pipe_read_callback;
#else
  buffer->callback = hin_pipe_read_callback;
  buffer->count = buffer->sz;
#endif

  buffer->pos = pipe->in.pos;
  if ((pipe->in.flags & HIN_COUNT) && pipe->left < READ_SZ) buffer->count = pipe->left;

  if (hin_request_read (buffer) < 0) {
    if (pipe->in_error_callback)
      pipe->in_error_callback (pipe);
    hin_pipe_close (pipe);
    return -1;
  }

  buffer->list.next = buffer->list.prev = NULL;
  basic_dlist_append (&pipe->reading, &buffer->list);

  return 0;
}

static int hin_pipe_write_next (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  buffer->fd = pipe->out.fd;
  buffer->ssl = pipe->out.ssl;
  buffer->flags = pipe->out.flags;
  buffer->callback = hin_pipe_write_callback;
  buffer->parent = pipe;

  buffer->pos = pipe->out.pos;

  if (pipe->out.flags & HIN_OFFSETS) {
    pipe->out.pos += buffer->count;
  }

  basic_dlist_remove (&pipe->write_que, &buffer->list);

  if (hin_request_write (buffer) < 0) {
    if (pipe->out_error_callback)
      pipe->out_error_callback (pipe);
    hin_pipe_close (pipe);
    return -1;
  }

  basic_dlist_append (&pipe->writing, &buffer->list);

  pipe->out.flags &= ~HIN_DONE;

  return 0;
}

int hin_pipe_advance (hin_pipe_t * pipe) {
  if ((pipe->in.flags & HIN_COUNT) && pipe->left <= 0) pipe->in.flags |= HIN_DONE;

  if (pipe->write_que.next == NULL
  && pipe->writing.next == NULL
  && ((pipe->in.flags & pipe->out.flags) & HIN_DONE)) {
    if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d close read %d write %d\n", pipe->in.fd, pipe->out.fd, (pipe->in.flags & HIN_DONE), (pipe->out.flags & HIN_DONE));
    hin_pipe_close (pipe);
    return 0;
  }

  if ((pipe->in.flags & (HIN_DONE|HIN_INACTIVE)) == 0
    && pipe->reading.next == NULL) {
    hin_buffer_t * new = pipe->buffer_callback (pipe, READ_SZ);
    if (hin_pipe_read_next (pipe, new) < 0) {
      hin_buffer_clean (new);
      return -1;
    }
  }

  if (pipe->write_que.next && (pipe->out.flags & HIN_DONE)) {
    hin_buffer_t * buffer = hin_buffer_list_ptr (pipe->write_que.next);
    if (hin_pipe_write_next (pipe, buffer) < 0) return -1;
  }
  return 0;
}

int hin_pipe_write_callback (hin_buffer_t * buffer, int ret) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  pipe->out.flags |= HIN_DONE;
  if (ret < 0) {
    printf ("pipe %d>%d write error '%s'\n", pipe->in.fd, pipe->out.fd, strerror (-ret));
    basic_dlist_remove (&pipe->writing, &buffer->list);
    if (pipe->out_error_callback)
      pipe->out_error_callback (pipe);
    hin_pipe_close (pipe);
    return -1;
  }
  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d write %d/%d pos %lld left %lld\n", pipe->in.fd, pipe->out.fd, ret, buffer->count, (long long)pipe->out.pos, (long long)pipe->left);
  pipe->count += ret;
  if (pipe->flags & HIN_HASH) {
    uint8_t * start = (uint8_t*)buffer->ptr;
    uint8_t * max = start + ret;
    while (start < max) {
      int c = *start++;
      pipe->hash = ((pipe->hash << 3) + pipe->hash) + c;
    }
  }
  if (ret < buffer->count) {
    if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d write incomplete %d/%d\n", pipe->in.fd, pipe->out.fd, ret, buffer->count);

    buffer->ptr += ret;
    buffer->count -= ret;
    if (buffer->flags & HIN_OFFSETS)
      buffer->pos += ret;

    if (hin_request_write (buffer) < 0) {
      basic_dlist_remove (&pipe->writing, &buffer->list);
      if (pipe->out_error_callback)
        pipe->out_error_callback (pipe);
      hin_pipe_close (pipe);
      return -1;
    }
    return 0;
  }

  basic_dlist_remove (&pipe->writing, &buffer->list);
  if (pipe->write_que.next) {
    hin_buffer_t * buffer = hin_buffer_list_ptr (pipe->write_que.next);
    hin_pipe_write_next (pipe, buffer);
  }
  hin_pipe_advance (pipe);
  return 1;
}

int hin_pipe_read_callback (hin_buffer_t * buffer, int ret) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  if (ret <= 0) {
    if (ret == 0 && (pipe->debug & DEBUG_PIPE)) {
      printf ("pipe %d>%d read EOF\n", pipe->in.fd, pipe->out.fd);
    } else if (ret < 0) {
      printf ("pipe %d>%d read error '%s'\n", pipe->in.fd, pipe->out.fd, strerror (-ret));
      basic_dlist_remove (&pipe->reading, &buffer->list);
      if (pipe->in_error_callback)
        pipe->in_error_callback (pipe);
    }

    pipe->in.flags |= HIN_DONE;
    ret = 0;
  }

  basic_dlist_remove (&pipe->reading, &buffer->list);

  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d read  %d/%d pos %lld left %lld %s\n",
    pipe->in.fd, pipe->out.fd, ret, buffer->count, (long long)pipe->in.pos, (long long)pipe->left, pipe->in.flags & HIN_DONE ? "flush" : "cont");

  buffer->count = ret;
  hin_pipe_process_buffer (pipe, buffer);

  if (pipe->write_que.next == NULL) {
    pipe->out.flags |= HIN_DONE;
  }
  hin_pipe_advance (pipe);
  #if USE_LINES
  return -1;
  #endif
  return 0;
}


