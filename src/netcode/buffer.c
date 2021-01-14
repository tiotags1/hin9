
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

void hin_buffer_clean (hin_buffer_t * buffer) {
  if (buffer->type == HIN_DYN_BUFFER) {
    if (buffer->data) free (buffer->data);
  }
  free (buffer);
}

void hin_buffer_list_remove (hin_buffer_t ** list, hin_buffer_t * new) {
  if (*list == new) {
    *list = new->next;
  } else {
    printf ("fatal error\n");
    exit (-1);
  }
  new->next = new->prev = NULL;
}

void hin_buffer_list_append (hin_buffer_t ** list, hin_buffer_t * new) {
  if (*list == NULL) {
    *list = new;
  } else {
    hin_buffer_t * last;
    for (last = *list; last->next; last = last->next) {}
    last->next = new;
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
}

int hin_buffer_eat (hin_buffer_t * buffer, int num) {
  int new_pos = buffer->ptr - buffer->data;

  if (num > 0) {
    memmove (buffer->data, buffer->data + num, new_pos - num);
    new_pos -= num;
  }

  buffer->ptr = buffer->data + new_pos;
  buffer->count += num;
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret);

int hin_lines_request (hin_buffer_t * buffer) {
  int sz;
  int new_pos = buffer->ptr - buffer->data;
  int left = buffer->sz - new_pos;
  if (left < (sz / 2)) { sz = READ_SZ; }
  else { sz = left; }
  hin_buffer_prepare (buffer, sz);
  buffer->callback = hin_lines_read_callback;
  hin_request_read (buffer);
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret) {
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  int err = 0;

  if (ret <= 0) {
    if (lines->close_callback)
      err = lines->close_callback (buffer);
    return err;
  }

  buffer->ptr += ret;
  buffer->count -= ret;

  int num = lines->read_callback (buffer);
  if (num < 0) {
    if (lines->close_callback) {
      err = lines->close_callback (buffer);
    } else {
      printf ("lines client error\n");
    }
    return err;
  }
  if (lines->eat_callback) {
    if (lines->eat_callback (buffer, num))
      hin_buffer_clean (buffer);
  } else {
    if (num > 0) {
      hin_buffer_eat (buffer, num);
    }
    if (num == 0)
      hin_lines_request (buffer);
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
  return buf;
}

