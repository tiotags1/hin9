
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hin.h"
#include "http.h"

static int done_file (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("pipe file transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  if (pipe->extra_callback) pipe->extra_callback (pipe);
  if (close (pipe->in.fd)) perror ("close in");

  httpd_client_finish_request (pipe->parent);
}

int hin_pipe_copy_deflate (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)client;

  int have, err;
  http->z.avail_in = num;
  http->z.next_in = buffer->buffer;
  buffer->count = num;

  if (master.debug & DEBUG_DEFLATE) printf ("deflate num %d flush %d\n", num, flush);
  char numbuf[10]; // size of max nr (7 bytes) + crlf + \0

  do {
    hin_buffer_t * new = hin_pipe_buffer_get (pipe);
    new->count = 0;
    if (http->peer_flags & HIN_HTTP_CHUNKED) {
      new->sz -= (sizeof (numbuf) + 8); // crlf + 0+crlfcrlf + \0
      new->count = sizeof (numbuf);
    }
    http->z.avail_out = new->sz;
    http->z.next_out = &new->buffer[new->count];
    int ret1 = deflate (&http->z, flush);
    have = new->sz - http->z.avail_out;

    if (have > 0) {
      new->count += have;
      new->fd = pipe->out.fd;

      if (http->peer_flags & HIN_HTTP_CHUNKED) {
        new->sz += sizeof (numbuf) + 8;
        header (client, new, "\r\n");
        if (flush && (http->z.avail_out != 0)) {
          header (client, new, "0\r\n\r\n");
        }

        int num = snprintf (numbuf, sizeof (numbuf), "%x\r\n", have);
        if (num < 0 || num >= sizeof (numbuf)) { printf ("weird error\n"); }
        int offset = sizeof (numbuf) - num;
        char * ptr = new->buffer + offset;
        memcpy (ptr, numbuf, num);
        new->ptr = ptr;
        new->count -= offset;
      }
      if (master.debug & DEBUG_DEFLATE) printf ("deflate write to pipe %d bytes %d total flush %d\n", have, new->count, flush);
      hin_pipe_write (pipe, new);
    } else {
      hin_buffer_clean (new);
    }
  } while (http->z.avail_out == 0);
  return 1;
}

int hin_pipe_copy_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)client;

  //if (num <= 0) return 0; // chunked requires it

  hin_buffer_t * buf = malloc (sizeof *buf + num + 50);
  memset (buf, 0, sizeof (*buf));
  buf->parent = (void*)pipe;
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = 0;
  buf->sz = num + 50;
  buf->ssl = pipe->ssl;

  //buffer->count = num;
  if (num > 0) {
    header (client, buf, "%x\r\n", num);

    memcpy (buf->ptr + buf->count, buffer->ptr, num);
    buf->count += num;

    header (client, buf, "\r\n");
  }

  if (flush) {
    header (client, buf, "0\r\n\r\n");
  }

  hin_pipe_write (pipe, buf);

  return 1;
}

hin_pipe_t * send_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *)) {
  httpd_client_t * http = (httpd_client_t*)client;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  pipe->in.fd = filefd;
  pipe->in.flags = HIN_OFFSETS;
  pipe->in.pos = pos;
  pipe->out.fd = client->sockfd;
  pipe->out.flags = HIN_DONE | HIN_SOCKET | (client->flags & HIN_SSL);
  pipe->out.pos = 0;
  pipe->parent = client;
  pipe->count = count;
  pipe->ssl = &client->ssl;
  pipe->read_callback = NULL;
  pipe->finish_callback = done_file;
  pipe->extra_callback = extra;

  int httpd_request_chunked (httpd_client_t * http);
  if (http->peer_flags & HIN_HTTP_CHUNKED) {
    if (httpd_request_chunked (http)) {
      pipe->read_callback = hin_pipe_copy_chunked;
    }
  }
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    httpd_request_chunked (http);
    pipe->read_callback = hin_pipe_copy_deflate;
  }

  hin_pipe_advance (pipe);

  return pipe;
}

int httpd_request_chunked (httpd_client_t * http);

int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe) {
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    httpd_request_chunked (http);
    pipe->read_callback = hin_pipe_copy_deflate;
  } else if (http->peer_flags & HIN_HTTP_CHUNKED) {
    if (httpd_request_chunked (http)) {
      pipe->read_callback = hin_pipe_copy_chunked;
    }
  }
}

static int done_receive_file (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("download file transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  if (pipe->extra_callback) pipe->extra_callback (pipe);
  if (close (pipe->out.fd)) perror ("close in");
}

hin_pipe_t * receive_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *)) {
  http_client_t * http = (http_client_t*)client;

  hin_pipe_t * pipe = calloc (1, sizeof (*pipe));
  pipe->in.fd = client->sockfd;
  pipe->in.flags = HIN_SOCKET;
  pipe->in.pos = 0;
  pipe->out.fd = filefd;
  pipe->out.flags = HIN_DONE | HIN_OFFSETS | (client->flags & HIN_SSL);
  pipe->out.pos = pos;
  pipe->parent = client;
  pipe->count = pipe->sz = count;
  pipe->ssl = &client->ssl;
  pipe->read_callback = NULL;
  pipe->finish_callback = done_receive_file;
  pipe->extra_callback = extra;

  hin_pipe_advance (pipe);

  return pipe;
}

