
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <hin.h>

static int done_file (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("pipe file transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  if (pipe->extra_callback) pipe->extra_callback (pipe);
  if (close (pipe->in.fd)) perror ("close in");
  httpd_client_finish_request (pipe->parent);
}

int hin_pipe_copy_deflate (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;

  int have, err;
  http->z.avail_in = num;
  http->z.next_in = buffer->buffer;
  buffer->count = num;

  do {
    hin_buffer_t * new = hin_pipe_buffer_get (pipe);
    char * ptr = new->ptr;
    if (http->flags & HIN_HTTP_CHUNKED) {
      new->sz -= 20; // size of max nr, 4 crlf + 0+crlfcrlf
      new->count = 0;
      int n = header (client, new, "%-8x\r\n", new->sz);
      ptr += n;
    }
    http->z.avail_out = new->sz;
    http->z.next_out = ptr;
    int ret1 = deflate (&http->z, flush);
    have = new->sz - http->z.avail_out;
    // add a write for new
    if (have > 0) {
      new->count = have;
      new->fd = pipe->out.fd;
      if (http->flags & HIN_HTTP_CHUNKED) {
        snprintf (new->buffer, 8, "%-8x", have);
        header (client, new, "\r\n");
        if (http->z.avail_out != 0) {
          header (client, new, "0\r\n\r\n");
        }
      }
      http_write (pipe, new);
    } else {
      hin_buffer_clean (new);
    }
  } while (http->z.avail_out == 0);
  hin_buffer_clean (buffer);
}

int hin_pipe_copy_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)&client->extra;

printf ("chunked num %d flush %d\n", num, flush);
  //if (num <= 0) return 0; // chunked requires it

  hin_buffer_t * buf = malloc (sizeof *buf + READ_SZ + 50);
  memset (buf, 0, sizeof (*buf));
  buf->parent = (void*)pipe;
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = 0;
  buf->sz = READ_SZ;
  buf->ssl = pipe->ssl;

  //buffer->count = num;
  header (client, buf, "%x\r\n", num);

  memcpy (buf->ptr + buf->count, buffer->ptr, num);
  buf->count += num;

  header (client, buf, "\r\n");

  if (flush && num > 0) {
    header (client, buf, "%x\r\n\r\n", 0);
  }

  http_write (pipe, buf);

  hin_buffer_clean (buffer);
}

hin_pipe_t * send_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *)) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;

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
  
  if (http->flags & HIN_HTTP_CHUNKED) {
    pipe->read_callback = hin_pipe_copy_chunked;
  }
  if (http->flags & HIN_HTTP_DEFLATE) {
    pipe->read_callback = hin_pipe_copy_deflate;
  }

  hin_pipe_advance (pipe);

  return pipe;
}

static int done_receive_file (hin_pipe_t * pipe) {
  if (master.debug & DEBUG_PIPE) printf ("download file transfer finished infd %d outfd %d\n", pipe->in.fd, pipe->out.fd);
  if (pipe->extra_callback) pipe->extra_callback (pipe);
  if (close (pipe->out.fd)) perror ("close in");
}

hin_pipe_t * receive_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *)) {
  http_client_t * http = (http_client_t*)&client->extra;

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

