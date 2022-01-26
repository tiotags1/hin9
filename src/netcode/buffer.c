
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"

void hin_buffer_clean (hin_buffer_t * buffer) {
  if (buffer->flags & HIN_ACTIVE) {
    printf ("error! buf still active %p\n", buffer);
  }

  if (buffer->debug & DEBUG_MEMORY) printf ("cleanup buffer %p\n", buffer);
  if (buffer->ssl_buffer) {
    hin_buffer_clean (buffer->ssl_buffer);
  }
  if (buffer->type == HIN_DYN_BUFFER) {
    hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
    if (lines->base) free (lines->base);
  }
  free (buffer);
}

void hin_buffer_stop_clean (hin_buffer_t * buf) {
  if (buf->flags & HIN_ACTIVE) {
    //printf ("error! buf still active %p\n", buf);
  }
  buf->flags &= ~HIN_ACTIVE;
  hin_buffer_clean (buf);
}

hin_buffer_t * hin_buffer_create_from_data (void * parent, const char * ptr, int sz) {
  hin_buffer_t * buf = malloc (sizeof *buf + sz);
  memset (buf, 0, sizeof (*buf));
  buf->count = buf->sz = sz;
  buf->ptr = buf->buffer;
  buf->parent = parent;
  memcpy (buf->ptr, ptr, sz);
  return buf;
}

int hin_buffer_continue_write (hin_buffer_t * buf, int ret) {
  if (ret <= 0) {

  } else if (ret != buf->count) {
    buf->ptr += ret;
    buf->count -= ret;
    if (buf->flags & HIN_OFFSETS)
      buf->pos += ret;

    if (hin_request_write (buf) < 0)
      printf ("error! %d\n", 3253534);

  } else if (buf->list.next) {
    hin_buffer_t * next = hin_buffer_list_ptr (buf->list.next);
    next->callback = buf->callback;
    next->parent = buf->parent;

    if (hin_request_write (next) < 0)
      printf ("error! %d\n", 3253534);
    hin_buffer_clean (buf);
  } else {
    return 0;
  }
  return ret;
}

int hin_buffer_prepare (hin_buffer_t * buffer, int num) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  int pos = lines->count;
  int min_sz = pos + num;
  if (min_sz > buffer->sz) {
    buffer->sz = min_sz;
    lines->base = realloc (lines->base, buffer->sz);
  }
  buffer->ptr = lines->base + pos;
  buffer->count = num;
  return 0;
}

int hin_buffer_eat (hin_buffer_t * buffer, int num) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  int pos = lines->count;
  char * base = lines->base;

  if (num < 0) return -1;

  memmove (base, base + num, pos - num);
  pos -= num;

  buffer->ptr = base + pos;
  lines->count -= num;
  return 0;
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret);

int hin_lines_request (hin_buffer_t * buffer, int min) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  int left = buffer->sz - lines->count;
  int new;

  if (buffer->flags & HIN_ACTIVE) {
    return 0;
  }
  if (min > 0 && min > left) { new = min - left; }
  else if (left < (buffer->sz / 2)) { new = READ_SZ; }
  else { new = left; }
  if (left < 0) {
    printf ("error! lines request negative %d - %d = %d\n", buffer->sz, lines->count, left);
  }
  if (new < 0) {
    printf ("error! lines new is %d: min %d left %d\n", new, min, left);
  }
  hin_buffer_prepare (buffer, new);
  buffer->callback = hin_lines_read_callback;
  if (buffer->fd >= 0) {
  if (hin_request_read (buffer) < 0) {
    printf ("error! %d\n", 3464354);
    return -1;
  }
  }
  return 0;
}

int hin_lines_default_eat (hin_buffer_t * buffer, int num) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  if (num > 0) {
    hin_buffer_eat (buffer, num);
  } else if (num == 0) {
    hin_lines_request (buffer, 0);
  } else {
    if (lines->close_callback) {
      return lines->close_callback (buffer, num);
    } else {
      weird_error ("lines close");
    }
    return -1;
  }
  return 0;
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  if (ret <= 0) {
    if (lines->close_callback)
      return lines->close_callback (buffer, ret);
    return -1;
  }

  buffer->count = 0;
  lines->count += ret;

  int num = lines->read_callback (buffer, ret);

  int ret1 = lines->eat_callback (buffer, num);
  return ret1;
}

int hin_lines_reread (hin_buffer_t * buf) {
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;

  buf->count = 0;

  int num = lines->read_callback (buf, lines->count);

  int ret = lines->eat_callback (buf, num);
  if (ret) hin_buffer_clean (buf);
  return ret;
}

int hin_lines_write (hin_buffer_t * buf, char * data, int len) {
  hin_buffer_prepare (buf, len);
  memcpy (buf->ptr, data, len);
  int ret = buf->callback (buf, len);
  if (ret) { hin_buffer_clean (buf); }
  return ret;
}

hin_buffer_t * hin_lines_create_raw (int sz) {
  hin_buffer_t * buf = calloc (1, sizeof *buf + sizeof (hin_lines_t));
  buf->type = HIN_DYN_BUFFER;
  buf->flags = 0;
  buf->count = buf->sz = sz;
  buf->pos = 0;
  buf->ptr = malloc (buf->sz);
  buf->callback = hin_lines_read_callback;

  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->eat_callback = hin_lines_default_eat;
  lines->base = buf->ptr;
  return buf;
}

