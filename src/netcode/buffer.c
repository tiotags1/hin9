
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

typedef struct {
  int (*callback) (hin_client_t * client, hin_buffer_t * source);
  int (*close_callback) (hin_buffer_t * buffer);
} hin_lines_t;

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

static int hin_lines_close_callback (hin_buffer_t * buffer, int ret) {
  if (ret < 0) {
    printf ("close fd %d had error %s\n", buffer->fd, strerror (-ret));
    return 1;
  }
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_client_close (client);
  return 1;
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

  int num = lines->callback (buffer->parent, buffer);
  if (num < 0) {
    if (lines->close_callback) {
      err = lines->close_callback (buffer);
    } else {
      printf ("lines client error\n");
    }
    return err;
  }
  if (num > 0) {
    hin_buffer_eat (buffer, num);
  }
  if (num == 0)
    hin_lines_request (buffer);

  return 0;
}

static int hin_lines_httpd_close_callback (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  buffer->callback = hin_lines_close_callback;
  hin_request_close (buffer);
  return 0;
}

hin_buffer_t * hin_lines_create (hin_client_t * client, int sockfd, int (*callback) (hin_client_t * client, hin_buffer_t * source)) {
  hin_buffer_t * buf = calloc (1, sizeof *buf + sizeof (hin_lines_t));
  buf->type = HIN_DYN_BUFFER;
  buf->flags = 0;
  buf->fd = sockfd;
  buf->callback = hin_lines_read_callback;
  buf->count = buf->sz = READ_SZ;
  buf->pos = 0;
  buf->data = malloc (buf->sz);
  buf->ptr = buf->data;
  buf->parent = client;
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->ssl = &client->ssl;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->callback = callback;
  lines->close_callback = hin_lines_httpd_close_callback;
  hin_request_read (buf);
  return buf;
}

static int hin_lines_cgi_close_callback (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  //buffer->callback = hin_lines_close_callback;
  //hin_request_close (buffer);
  //printf ("cgi close headers\n");
  return 1;
}

hin_buffer_t * hin_lines_create_cgi (hin_client_t * client, int sockfd, int (*callback) (hin_client_t * client, hin_buffer_t * source)) {
  hin_buffer_t * buf = calloc (1, sizeof *buf + sizeof (hin_lines_t));
  buf->type = HIN_DYN_BUFFER;
  buf->fd = sockfd;
  buf->callback = hin_lines_read_callback;
  buf->count = buf->sz = READ_SZ;
  buf->pos = 0;
  buf->data = malloc (buf->sz);
  buf->ptr = buf->data;
  buf->parent = client;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->callback = callback;
  lines->close_callback = hin_lines_cgi_close_callback;
  hin_request_read (buf);
  return buf;
}

