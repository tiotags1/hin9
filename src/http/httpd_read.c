
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include "hin.h"
#include "lua.h"
#include "conf.h"

int httpd_client_read_callback (hin_buffer_t * buffer);

int httpd_client_reread (hin_client_t * client) {
  hin_buffer_t * buffer = client->read_buffer;

  string_t source;
  source.ptr = buffer->data;
  source.len = buffer->ptr - buffer->data;
  int num = 0;
  if (source.len > 0)
    num = httpd_client_read_callback (buffer);
  if (num < 0) {
    printf ("client error\n");
    httpd_client_shutdown (client);
    return -1;
  } else if (num > 0) {
    hin_buffer_eat (buffer, num);
  } else {
    buffer->count = buffer->sz;
    hin_lines_request (buffer);
  }
}

#include <sys/stat.h>
#include <fcntl.h>
int httpd_client_finish (hin_client_t * client);
int post_done (hin_pipe_t * pipe) {
  printf ("post done %d\n", pipe->out.fd);
  //close (pipe->out);
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)client->extra;
  http->state &= ~HIN_REQ_POST;
  httpd_client_finish (client);
}

int httpd_client_read_callback (hin_buffer_t * buffer) {
  string_t source1, * source = &source1;
  source->ptr = buffer->data;
  source->len = buffer->ptr - buffer->data;

  hin_client_t * client = (hin_client_t*)buffer->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;

  if (source->len >= HIN_HTTPD_MAX_HEADER_SIZE) {
    httpd_respond_error (client, 413, NULL);
    httpd_client_shutdown (client);
    return -1;
  }

  int used = httpd_parse_req (client, source);
  if (used <= 0) return used;
  if (http->post_sz > 0) {
    int consume = source->len > http->post_sz ? http->post_sz : source->len;
    off_t left = http->post_sz - consume;
    used += consume;
    // send post data in buffer to post handler
    http->post_fd =  openat (AT_FDCWD, HIN_HTTPD_POST_DIRECTORY, O_RDWR | O_TMPFILE, 0600);
    if (http->post_fd < 0) { printf ("openat tmpfile failed %s\n", strerror (errno)); }
    printf ("post initial %ld %ld fd %d '%.*s'\n", http->post_sz, source->len, http->post_fd, consume, source->ptr);
    if (write (http->post_fd, source->ptr, consume) < 0)
      perror ("write");
    if (left > 0) {
      http->state |= HIN_REQ_POST;
      // request more post
      receive_file (client, http->post_fd, consume, left, 0, post_done);
      //return used;
    } else {
      //close (http->post_fd);
    }
  }
  http->status = 200;
  int hin_server_callback (hin_client_t * client);
  hin_server_callback (client);

  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    int hin_client_deflate_init (httpd_client_t * http);
    hin_client_deflate_init (http);
  }

  if ((http->state & ~(HIN_REQ_HEADERS|HIN_REQ_END)) == 0) {
    printf ("httpd 500 missing request\n");
    httpd_respond_error (client, 500, NULL);
    httpd_client_shutdown (client);
    return used;
  }
  if (http->state & HIN_REQ_HEADERS) {
    httpd_client_reread (client);
  }
  return used;
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

static int hin_lines_httpd_close_callback (hin_buffer_t * buffer) {
  hin_client_t * client = (hin_client_t*)buffer->parent;
  hin_lines_t * lines = (hin_lines_t*)&buffer->buffer;
  buffer->callback = hin_lines_close_callback;
  hin_request_close (buffer);
  return 0;
}

hin_buffer_t * hin_lines_create (hin_client_t * client) {
  hin_buffer_t * buf = hin_lines_create_raw ();
  buf->fd = client->sockfd;
  buf->parent = client;
  buf->flags = HIN_SOCKET | (client->flags & HIN_SSL);
  buf->ssl = &client->ssl;
  hin_lines_t * lines = (hin_lines_t*)&buf->buffer;
  lines->read_callback = httpd_client_read_callback;
  lines->close_callback = hin_lines_httpd_close_callback;
  hin_request_read (buf);
  return buf;
}



