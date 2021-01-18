
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zlib.h>

#include "hin.h"

int hin_pipe_read_callback (hin_buffer_t * buffer, int ret);
int hin_pipe_write_callback (hin_buffer_t * buffer, int ret);

void hin_pipe_close (hin_pipe_t * pipe) {
  if (pipe->finish_callback) pipe->finish_callback (pipe);
  free (pipe);
}

hin_buffer_t * hin_pipe_buffer_get (hin_pipe_t * pipe) {
  hin_buffer_t * buf = malloc (sizeof *buf + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  buf->parent = (void*)pipe;
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = buf->sz = READ_SZ;
  buf->ssl = pipe->ssl;
  return buf;
}

int hin_pipe_write (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  hin_buffer_list_append (&pipe->write, buffer);
}

static int hin_pipe_read_next (hin_buffer_t * buffer) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  buffer->fd = pipe->in.fd;
  buffer->ssl = pipe->ssl;
  buffer->flags = pipe->in.flags & (HIN_SOCKET | HIN_SSL);
  buffer->callback = hin_pipe_read_callback;

  buffer->pos = pipe->in.pos;
  buffer->count = READ_SZ < pipe->count || pipe->count == 0 ? READ_SZ : pipe->count;
  buffer->sz = READ_SZ;

  hin_request_read (buffer);
}

static int hin_pipe_write_next (hin_buffer_t * buffer) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  buffer->fd = pipe->out.fd;
  buffer->ssl = pipe->ssl;
  buffer->flags = pipe->out.flags & (HIN_SOCKET | HIN_SSL);
  buffer->callback = hin_pipe_write_callback;

  buffer->pos = pipe->out.pos;

  hin_request_write (buffer);
}

int hin_pipe_advance (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("pipe %d>%d advance\n", pipe->in.fd, pipe->out.fd);
  if (pipe->write == NULL && (pipe->flags & HIN_DONE)) {
    hin_pipe_close (pipe);
    return 0;
  }

  if (pipe->out.flags & HIN_DONE) {
    hin_buffer_t * new = NULL;
    if (pipe->buffer_callback == NULL) {
      new = hin_pipe_buffer_get (pipe);
    } else {
      new = pipe->buffer_callback (pipe);
    }
    hin_pipe_read_next (new);
    pipe->out.flags &= (~HIN_DONE);
  }

  if (pipe->write) {
    hin_buffer_t * buffer = pipe->write;
    hin_pipe_write_next (buffer);
    hin_buffer_list_remove (&pipe->write, buffer);
  }
}

int hin_pipe_write_callback (hin_buffer_t * buffer, int ret) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  if (ret < 0) {
    printf ("pipe %d>%d write error '%s'\n", pipe->in.fd, pipe->out.fd, strerror (-ret));
    if (pipe->out_error_callback)
      pipe->out_error_callback (pipe);
    hin_pipe_close (pipe);
    return -1;
  }
  if (master.debug & DEBUG_PIPE) printf ("pipe %d>%d write %d/%d left %ld\n", pipe->in.fd, pipe->out.fd, ret, buffer->count, pipe->count);
  if (pipe->out.flags & HIN_OFFSETS) {
    pipe->out.pos += ret;
  }
  if (ret < buffer->count) {
    printf ("pipe %d>%d write incomplete %d/%d\n", pipe->in.fd, pipe->out.fd, ret, buffer->count);
    buffer->ptr += ret;
    buffer->count -= ret;
    hin_request_write (buffer);
    return 0;
  }
  if (pipe->write) {
    hin_buffer_t * buffer = pipe->write;
    hin_pipe_write_next (buffer);
    hin_buffer_list_remove (&pipe->write, buffer);
  } else {
    pipe->out.flags |= HIN_DONE;
    hin_pipe_advance (pipe);
  }
  hin_buffer_clean (buffer);
  return 0;
}

int hin_pipe_copy_raw (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  if (num <= 0) return 1;
  buffer->count = num;
  hin_pipe_write (pipe, buffer);
  //if (flush) return 1; // already cleaned in the write done handler
  return 0;
}

int hin_pipe_finish (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("pipe %d>%d finish function\n", pipe->in.fd, pipe->out.fd);
  pipe->flags |= HIN_DONE;
  pipe->in.flags |= HIN_DONE;
  //hin_pipe_advance (pipe);
}

int hin_pipe_append (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  if (master.debug & DEBUG_PIPE) printf ("pipe %d>%d append\n", pipe->in.fd, pipe->out.fd);
  int ret1 = 0;
  if (pipe->read_callback == NULL) {
    ret1 = hin_pipe_copy_raw (pipe, buffer, buffer->count, pipe->flags & HIN_DONE ? 1 : 0);
  } else {
    ret1 = pipe->read_callback (pipe, buffer, buffer->count, pipe->flags & HIN_DONE ? 1 : 0);
  }
  if (ret1) hin_buffer_clean (buffer);
}

int hin_pipe_read_callback (hin_buffer_t * buffer, int ret) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  if (ret <= 0) {
    if (ret == 0 && master.debug & DEBUG_PIPE) {
      printf ("pipe %d>%d read EOF\n", pipe->in.fd, pipe->out.fd);
    }
    if (ret < 0) {
      printf ("pipe %d>%d read error\n", pipe->in.fd, pipe->out.fd);
      if (pipe->in_error_callback)
        pipe->in_error_callback (pipe);
    }

    pipe->flags |= HIN_DONE;
    pipe->in.flags |= HIN_DONE;
    ret = 0;
  }
  if (pipe->in.flags & HIN_OFFSETS) {
    pipe->in.pos += ret;
  }
  if (pipe->count > 0) {
    pipe->count -= ret;
    if (pipe->count <= 0) {
      if (master.debug & DEBUG_PIPE) printf ("pipe %d>%d read finished\n", pipe->in.fd, pipe->out.fd);
      pipe->in.flags |= HIN_DONE;
      pipe->flags |= HIN_DONE;
    }
  }

  if (master.debug & DEBUG_PIPE) printf ("pipe %d>%d read  %d/%d left %ld%s\n",
    pipe->in.fd, pipe->out.fd, ret, buffer->count, pipe->count, pipe->flags & HIN_DONE ? " done" : "");

  int ret1 = 0;
  if (pipe->read_callback == NULL) {
    ret1 = hin_pipe_copy_raw (pipe, buffer, ret, pipe->flags & HIN_DONE ? 1 : 0);
  } else {
    ret1 = pipe->read_callback (pipe, buffer, ret, pipe->flags & HIN_DONE ? 1 : 0);
  }
  if (ret1) hin_buffer_clean (buffer);

  if (pipe->write == NULL) {
    pipe->out.flags |= HIN_DONE;
  }
  hin_pipe_advance (pipe);
  return 0;
}


