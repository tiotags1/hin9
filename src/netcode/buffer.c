
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"

void hin_buffer_clean (hin_buffer_t * buffer) {
  if (buffer->debug & DEBUG_MEMORY) printf ("cleanup buffer %p\n", buffer);
  if (buffer->ssl_buffer) {
    hin_buffer_clean (buffer->ssl_buffer);
  }
  if (buffer->type == HIN_DYN_BUFFER) {
    if (buffer->data) free (buffer->data);
  }
  free (buffer);
}

hin_buffer_t * hin_buffer_create_from_data (void * parent, const char * ptr, int sz) {
  hin_buffer_t * buf = calloc (1, sizeof *buf + sz);
  memset (buf, 0, sizeof (*buf));
  buf->count = buf->sz = sz;
  buf->ptr = buf->buffer;
  buf->parent = parent;
  memcpy (buf->ptr, ptr, sz);
  return buf;
}

void hin_buffer_list_remove (hin_buffer_t ** list, hin_buffer_t * new) {
  if (*list == new) {
    *list = new->next;
  } else {
    if (new->next)
      new->next->prev = new->prev;
    if (new->prev)
      new->prev->next = new->next;
  }
  new->next = new->prev = NULL;
}

void hin_buffer_list_add (hin_buffer_t ** list, hin_buffer_t * new) {
  new->next = new->prev = NULL;
  if (*list == NULL) {
    *list = new;
  } else {
    new->next = *list;
    (*list)->prev = new;
    *list = new;
  }
}

void hin_buffer_list_append (hin_buffer_t ** list, hin_buffer_t * new) {
  new->next = new->prev = NULL;
  if (*list == NULL) {
    *list = new;
  } else {
    hin_buffer_t * last;
    for (last = *list; last->next; last = last->next) {}
    last->next = new;
    new->prev = last;
  }
}

int hin_buffer_prepare (hin_buffer_t * buffer, int num) {
  int new_pos = buffer->ptr - buffer->data;
  int min_sz = new_pos + num;
  if (min_sz > buffer->sz) {
    buffer->sz = min_sz;
    buffer->data = realloc (buffer->data, buffer->sz);
  }
  buffer->ptr = buffer->data + new_pos;
  buffer->count = num;
  return 0;
}

int hin_buffer_eat (hin_buffer_t * buffer, int num) {
  int new_pos = buffer->ptr - buffer->data;

  if (num > 0) {
    memmove (buffer->data, buffer->data + num, new_pos - num);
    new_pos -= num;
  }

  buffer->ptr = buffer->data + new_pos;
  buffer->count += num;
  return 0;
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret);

int hin_lines_request (hin_buffer_t * buffer) {
  int sz = buffer->sz;
  int new_pos = buffer->ptr - buffer->data;
  int left = buffer->sz - new_pos;
  if (left < (sz / 2)) { sz = READ_SZ; }
  else { sz = left; }
  hin_buffer_prepare (buffer, sz);
  buffer->callback = hin_lines_read_callback;
  if (hin_request_read (buffer) < 0) {
    printf ("lines request failed\n");
    return -1;
  }
  return 0;
}

int hin_lines_default_eat (hin_buffer_t * buffer, int num) {
  if (num > 0) {
    hin_buffer_eat (buffer, num);
  } else if (num == 0) {
    hin_lines_request (buffer);
  } else {
    hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
    if (lines->close_callback) {
      return lines->close_callback (buffer);
    } else {
      printf ("lines client error\n");
    }
    return -1;
  }
  return 0;
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;

  if (ret <= 0) {
    if (lines->close_callback)
      return lines->close_callback (buffer);
    return -1;
  }

  buffer->ptr += ret;
  buffer->count -= ret;

  int num = lines->read_callback (buffer);

  if (lines->eat_callback (buffer, num)) {
    hin_buffer_clean (buffer);
  }

  return 0;
}

hin_buffer_t * hin_lines_create_raw () {
  hin_buffer_t * buf = calloc (1, sizeof *buf + sizeof (hin_lines_t));
  buf->type = HIN_DYN_BUFFER;
  buf->flags = 0;
  buf->count = buf->sz = READ_SZ;
  buf->pos = 0;
  buf->data = malloc (buf->sz);
  buf->ptr = buf->data;
  buf->callback = hin_lines_read_callback;

  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->eat_callback = hin_lines_default_eat;
  return buf;
}

