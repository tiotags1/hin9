

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hin.h>

typedef struct {
  int (*callback) (hin_client_t * client, string_t * source);
} hin_lines_t;

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret);

int hin_lines_request (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_buffer_prepare (buffer, READ_SZ);
  buffer->flags = HIN_SOCKET | (client->flags & HIN_SSL);
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
  //printf ("closed sockfd %d\n");
  return 1;
}

static int hin_lines_read_callback (hin_buffer_t * buffer, int ret) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;

  if (ret <= 0) {
    //printf ("lines received negative %d\n", ret);
    buffer->fd = client->sockfd;
    buffer->callback = hin_lines_close_callback;
    hin_request_close (buffer);
    //close (client->sockfd);
    //hin_client_close (client);
    return 0;
  }

  buffer->ptr += ret;
  buffer->count -= ret;

  string_t source;
  source.ptr = buffer->data;
  source.len = buffer->ptr - buffer->data;
  int num = lines->callback (buffer->parent, &source);
  if (num < 0) {
    printf ("lines client error\n");
    hin_client_shutdown (client);
    return -1;
  }
  if (num > 0) {
    hin_buffer_eat (buffer, num);
  }
  if (num == 0)
    hin_lines_request (buffer);
  return 0;
}

hin_buffer_t * hin_lines_create (hin_client_t * client, int sockfd, int (*callback) (hin_client_t * client, string_t * source)) {
  hin_buffer_t * buf = calloc (1, sizeof *buf + sizeof (hin_lines_t));
  buf->type = HIN_DYN_BUFFER;
  buf->flags = 0;
  buf->fd = sockfd;
  buf->callback = hin_lines_read_callback;
  buf->count = READ_SZ;
  buf->sz = READ_SZ;
  buf->pos = 0;
  buf->data = malloc (buf->sz);
  buf->ptr = buf->data;
  buf->parent = client;
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->ssl = &client->ssl;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->callback = callback;
  hin_request_read (buf);
  return buf;
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


