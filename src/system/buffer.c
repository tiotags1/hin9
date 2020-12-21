
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

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

void hin_buffer_clean (hin_buffer_t * buffer) {
  if (buffer->type == HIN_DYN_BUFFER) {
    if (buffer->data) free (buffer->data);
  }
  free (buffer);
}

void http_list_remove (hin_buffer_t ** list, hin_buffer_t * new) {
  if (*list == new) {
    *list = new->next;
  } else {
    printf ("fatal error\n");
    exit (-1);
  }
}

void http_list_append (hin_buffer_t ** list, hin_buffer_t * new) {
  if (*list == NULL) {
    *list = new;
  } else {
    hin_buffer_t * last;
    for (last = *list; last->next; last = last->next) {}
    last->next = new;
  }
}

int http_write (hin_pipe_t * pipe, hin_buffer_t * buffer) {
  http_list_append (&pipe->write, buffer);
}


