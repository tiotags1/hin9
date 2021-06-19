
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zlib.h>

#include "hin.h"

int hin_pipe_read_callback (hin_buffer_t * buffer, int ret);
int hin_pipe_write_callback (hin_buffer_t * buffer, int ret);
int hin_pipe_copy_raw (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);

void hin_pipe_close (hin_pipe_t * pipe) {
  if (pipe->finish_callback) pipe->finish_callback (pipe);
  if (pipe->extra) free (pipe->extra);
  free (pipe);
}

hin_buffer_t * hin_pipe_get_buffer (hin_pipe_t * pipe, int sz) {
  hin_buffer_t * buf = malloc (sizeof *buf + sz);
  memset (buf, 0, sizeof (*buf));
  buf->parent = (void*)pipe;
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = buf->sz = sz;
  buf->debug = pipe->debug;
  return buf;
}

int hin_pipe_write (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  hin_buffer_t * next = buffer->next;
  for (hin_buffer_t * buf = buffer; buf; buf = next) {
    next = buf->next;
    hin_buffer_list_append (&pipe->write, buf);
    pipe->num_write++;
  }
  return 0;
}

int hin_pipe_append (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  if (pipe->in.flags & HIN_COUNT) {
    pipe->left -= buffer->count;
    if (pipe->left <= 0) {
      pipe->in.flags |= HIN_DONE;
    }
  }

  int flush = (pipe->in.flags & HIN_DONE) ? 1 : 0;
  int ret1 = 0;

  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d append %d bytes %s\n", pipe->in.fd, pipe->out.fd, buffer->count, flush ? "flush" : "cont");

  if (pipe->decode_callback) {
    ret1 = pipe->decode_callback (pipe, buffer, buffer->count, flush);
  } else {
    ret1 = pipe->read_callback (pipe, buffer, buffer->count, flush);
  }
  if (ret1) hin_buffer_clean (buffer);
  return 0;
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
  hin_pipe_advance (pipe);
  return 0;
}

void hin_pipe_handled_read (hin_pipe_t * pipe) {
  pipe->num_read++;
}

int hin_pipe_copy_raw (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  if (num <= 0) return 1;
  buffer->count = num;
  hin_pipe_write (pipe, buffer);
  //if (flush) return 1; // already cleaned in the write done handler
  return 0;
}

static int hin_pipe_read_next (hin_buffer_t * buffer) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  if (pipe->flags & HIN_NO_READ) return -1;

  buffer->fd = pipe->in.fd;
  buffer->ssl = pipe->in.ssl;
  buffer->flags = pipe->in.flags;
  buffer->callback = hin_pipe_read_callback;

  buffer->pos = pipe->in.pos;
  buffer->count = buffer->sz = READ_SZ;
  if ((pipe->in.flags & HIN_COUNT) && pipe->left < READ_SZ) buffer->count = pipe->left;

  pipe->num_read++;

  if (hin_request_read (buffer) < 0) {
    if (pipe->in_error_callback)
      pipe->in_error_callback (pipe);
    hin_pipe_close (pipe);
    return -1;
  }
  return 0;
}

static int hin_pipe_write_next (hin_buffer_t * buffer) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  buffer->fd = pipe->out.fd;
  buffer->ssl = pipe->out.ssl;
  buffer->flags = pipe->out.flags;
  buffer->callback = hin_pipe_write_callback;
  buffer->next = buffer->prev = NULL;

  buffer->pos = pipe->out.pos;

  if (pipe->out.flags & HIN_OFFSETS) {
    pipe->out.pos += buffer->count;
  }

  if (hin_request_write (buffer) < 0) {
    if (pipe->out_error_callback)
      pipe->out_error_callback (pipe);
    hin_pipe_close (pipe);
    return -1;
  }
  return 0;
}

int hin_pipe_advance (hin_pipe_t * pipe) {
  if ((pipe->in.flags & HIN_COUNT) && pipe->left <= 0) pipe->in.flags |= HIN_DONE;

  if (pipe->write == NULL && (pipe->in.flags & HIN_DONE)) {
    if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d close read %d write %d\n", pipe->in.fd, pipe->out.fd, (pipe->in.flags & HIN_DONE), pipe->write ? 1 : 0);
    hin_pipe_close (pipe);
    return 0;
  }

  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d advance done %d read %d write %d\n", pipe->in.fd, pipe->out.fd, pipe->in.flags & HIN_DONE, pipe->num_read, pipe->num_write);

  if ((pipe->in.flags & HIN_DONE) == 0 && pipe->num_read < 1 && pipe->num_write < 1) {
    hin_buffer_t * new = pipe->buffer_callback (pipe, READ_SZ);
    if (hin_pipe_read_next (new) < 0) {
      hin_buffer_clean (new);
      return -1;
    }
  }

  if (pipe->write) {
    hin_buffer_t * buffer = pipe->write;
    hin_buffer_list_remove (&pipe->write, buffer);
    if (hin_pipe_write_next (buffer) < 0) return -1;
  }
  return 0;
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
      if (pipe->out_error_callback)
        pipe->out_error_callback (pipe);
      hin_pipe_close (pipe);
    }
    return 0;
  }
  pipe->num_write--;
  if (pipe->write) {
    hin_buffer_t * buffer = pipe->write;
    hin_buffer_list_remove (&pipe->write, buffer);
    hin_pipe_write_next (buffer);
  } else {
    hin_pipe_advance (pipe);
  }
  hin_buffer_clean (buffer);
  return 0;
}

int hin_pipe_read_callback (hin_buffer_t * buffer, int ret) {
  hin_pipe_t * pipe = (hin_pipe_t*)buffer->parent;
  if (ret <= 0) {
    if (ret == 0 && (pipe->debug & DEBUG_PIPE)) {
      printf ("pipe %d>%d read EOF\n", pipe->in.fd, pipe->out.fd);
    } else if (ret < 0) {
      printf ("pipe %d>%d read error '%s'\n", pipe->in.fd, pipe->out.fd, strerror (-ret));
      if (pipe->in_error_callback)
        pipe->in_error_callback (pipe);
    }

    pipe->in.flags |= HIN_DONE;
    ret = 0;
  }

  pipe->num_read--;

  if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d read  %d/%d pos %lld left %lld %s\n",
    pipe->in.fd, pipe->out.fd, ret, buffer->count, (long long)pipe->in.pos, (long long)pipe->left, pipe->in.flags & HIN_DONE ? "flush" : "cont");

  if (pipe->in.flags & HIN_OFFSETS) {
    pipe->in.pos += ret;
  }
  if (pipe->in.flags & HIN_COUNT) {
    pipe->left -= ret;
    if (pipe->left <= 0) {
      if (pipe->debug & DEBUG_PIPE) printf ("pipe %d>%d read finished\n", pipe->in.fd, pipe->out.fd);
      pipe->in.flags |= HIN_DONE;
    }
  }

  int ret1 = 0, flush = pipe->in.flags & HIN_DONE ? 1 : 0;
  if (pipe->decode_callback) {
    ret1 = pipe->decode_callback (pipe, buffer, ret, flush);
  } else {
    ret1 = pipe->read_callback (pipe, buffer, ret, flush);
  }
  if (ret1) hin_buffer_clean (buffer);

  if (pipe->write == NULL) {
    pipe->out.flags |= HIN_DONE;
  }
  hin_pipe_advance (pipe);
  return 0;
}


