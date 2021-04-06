
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hin.h"
#include "http.h"

void hin_pipe_handled_read (hin_pipe_t * pipe);

typedef struct {
  off_t chunk_sz;
  int left_over;
} hin_pipe_chunked_decode_t;

static void hin_pipe_decode_prepare_half_read (hin_pipe_t * pipe, hin_buffer_t * buffer, int left, int num) {
  hin_pipe_chunked_decode_t * decode = pipe->extra;
  decode->left_over = left;
  printf ("chunk needs more space left %d num %d\n", left, num);
  buffer->count = buffer->sz;
  if (left > 0) {
    memmove (buffer->ptr, buffer->ptr + num - left, left);
    buffer->count -= left;
  }
  printf ("left in buffer is '%.*s'\n", left, buffer->ptr);
  hin_pipe_handled_read (pipe);
  // offset should increase ?
  if (hin_request_read (buffer) < 0) {
    // TODO what's the proper way to handle failing here
    return ;
  }
}

int hin_pipe_decode_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_pipe_chunked_decode_t * decode = pipe->extra;
  if (decode == NULL) {
    decode = calloc (1, sizeof (*decode));
    pipe->extra = decode;
  }
  string_t source, orig, param1;
  orig.ptr = buffer->ptr - decode->left_over;
  orig.len = num + decode->left_over;
  source = orig;
  if (pipe->debug & DEBUG_HTTP_FILTER) printf ("pipe %d>%d decode chunk sz %d left %d %s\n", pipe->in.fd, pipe->out.fd, num, decode->left_over, flush ? "flush" : "cont");
  while (1) {
    if (pipe->debug & DEBUG_HTTP_FILTER) printf ("  chunk sz left %ld\n", decode->chunk_sz);
    if (decode->chunk_sz > 0) {
      int consume = decode->chunk_sz;
      if (consume > source.len) consume = source.len;
      if (pipe->debug & DEBUG_HTTP_FILTER) printf ("  chunk consume %d\n", consume);
      hin_buffer_t * buf = hin_pipe_get_buffer (pipe, consume);
      memcpy (buf->ptr, source.ptr, consume);
      if (pipe->read_callback (pipe, buf, consume, 0))
        hin_buffer_clean (buf);
      if (decode->chunk_sz > consume) {
        // want more;
        decode->chunk_sz -= consume;
        return 1;
      }
      source.ptr += consume;
      source.len -= consume;
      decode->chunk_sz -= consume;
      if (source.len < 2) {
        hin_pipe_decode_prepare_half_read (pipe, buffer, source.len-2, num);
        return 0;
      }
      if (match_string (&source, "\r\n") < 0) {
        printf ("chunk decode format error\n");
        return -1;
      }
    }
    int err = 0;
    if ((err = match_string (&source, "(%x+)\r\n", &param1)) <= 0) {
      // save stuff
      if (source.len < 10) {
        hin_pipe_decode_prepare_half_read (pipe, buffer, source.len, num);
        return 0;
      }
      printf ("chunk decode couldn't find in '%.*s'\n", (int)(source.len > 20 ? 20 : source.len), source.ptr);
      return -1;
    }
    decode->chunk_sz = strtol (param1.ptr, NULL, 16);
    if (pipe->debug & DEBUG_HTTP_FILTER) printf ("  chunk sz %ld found\n", decode->chunk_sz);
    if (decode->chunk_sz == 0 && param1.len > 0) {
      int ret = pipe->read_callback (pipe, buffer, 0, 1);
      pipe->in.flags |= HIN_DONE;
      if (ret) {}
      return 1;
    }
  }
  return 0;
}

int hin_pipe_copy_deflate (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_client_t * client = (hin_client_t*)pipe->parent;
  httpd_client_t * http = (httpd_client_t*)client;

  int have;
  http->z.avail_in = num;
  http->z.next_in = (Bytef *)buffer->buffer;
  buffer->count = num;

  if (http->debug & DEBUG_HTTP_FILTER) printf ("httpd %d deflate num %d flush %d\n", http->c.sockfd, num, flush);
  char numbuf[10]; // size of max nr (7 bytes) + crlf + \0

  do {
    hin_buffer_t * new = hin_pipe_get_buffer (pipe, READ_SZ);
    new->count = 0;
    if (http->peer_flags & HIN_HTTP_CHUNKED) {
      new->sz -= (sizeof (numbuf) + 8); // crlf + 0+crlfcrlf + \0
      new->count = sizeof (numbuf);
    }
    http->z.avail_out = new->sz;
    http->z.next_out = (Bytef *)&new->buffer[new->count];
    deflate (&http->z, flush);
    have = new->sz - http->z.avail_out;

    if (have > 0) {
      new->count += have;
      new->fd = pipe->out.fd;

      if (http->peer_flags & HIN_HTTP_CHUNKED) {
        new->sz += sizeof (numbuf) + 8;
        header (new, "\r\n");
        if (flush && (http->z.avail_out != 0)) {
          header (new, "0\r\n\r\n");
        }

        int num = snprintf (numbuf, sizeof (numbuf), "%x\r\n", have);
        if (num < 0 || num >= sizeof (numbuf)) { printf ("weird error\n"); }
        int offset = sizeof (numbuf) - num;
        char * ptr = new->buffer + offset;
        memcpy (ptr, numbuf, num);
        new->ptr = ptr;
        new->count -= offset;
      }
      if (http->debug & DEBUG_HTTP_FILTER) printf ("  deflate write %d total %d %s\n", have, new->count, flush ? "flush" : "cont");
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

  if (http->debug & DEBUG_HTTP_FILTER) printf ("httpd %d chunked num %d flush %d\n", http->c.sockfd, num, flush);

  hin_buffer_t * buf = malloc (sizeof *buf + num + 50);
  memset (buf, 0, sizeof (*buf));
  buf->parent = (void*)pipe;
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = 0;
  buf->sz = num + 50;
  buf->ssl = pipe->out.ssl;
  buf->debug = buffer->debug;

  //buffer->count = num;
  if (num > 0) {
    header (buf, "%x\r\n", num);

    memcpy (buf->ptr + buf->count, buffer->ptr, num);
    buf->count += num;

    header (buf, "\r\n");
  }

  if (flush) {
    header (buf, "0\r\n\r\n");
  }

  hin_pipe_write (pipe, buf);

  return 1;
}

int httpd_request_chunked (httpd_client_t * http);

int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe) {
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    httpd_request_chunked (http);
    pipe->read_callback = hin_pipe_copy_deflate;
  } else if ((http->peer_flags & HIN_HTTP_CHUNKED) || (http->count == 0)) {
    if (httpd_request_chunked (http)) {
      pipe->read_callback = hin_pipe_copy_chunked;
    }
  }
  return 0;
}

int httpd_pipe_upload_chunked (httpd_client_t * http, hin_pipe_t * pipe) {
  if (http->disable & HIN_HTTP_CHUNKED_UPLOAD) return 0;
  if (http->peer_flags & HIN_HTTP_CHUNKED_UPLOAD) {
    pipe->decode_callback = hin_pipe_decode_chunked;
    pipe->read_callback = hin_pipe_copy_chunked;
  }
  return 0;
}

