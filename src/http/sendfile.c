
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hin.h"
#include "http.h"

int hin_pipe_decode_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  string_t source, orig, param1, param2;
  orig.ptr = buffer->ptr;
  orig.len = num;
  source = orig;
  while (1) {
    if (master.debug & DEBUG_CHUNK) printf ("chunk sz left %ld\n", pipe->extra_sz);
    if (pipe->extra_sz > 0) {
      int consume = pipe->extra_sz;
      if (consume > source.len) consume = source.len;
      if (master.debug & DEBUG_CHUNK) printf ("chunk consume %d\n", consume);
      hin_buffer_t * buf = hin_pipe_get_buffer (pipe, consume);
      memcpy (buf->ptr, source.ptr, consume);
      if (pipe->read_callback (pipe, buf, consume, 0))
        hin_buffer_clean (buf);
      if (pipe->extra_sz > consume) {
        // want more;
        pipe->extra_sz -= consume;
        return 1;
      }
      source.ptr += consume;
      source.len -= consume;
      pipe->extra_sz -= consume;
      if (match_string (&source, "\r\n") < 0) {
        printf ("chunk format error\n");
        return -1;
      }
    }
    int err = 0;
    if ((err = match_string (&source, "(%x+)\r\n", &param1)) <= 0) {
      // save stuff
      if (source.len < 10) return 1;
      printf ("chunk couldn't find in '%.*s'\n", (int)(source.len > 20 ? 20 : source.len), source.ptr);
      return -1;
    }
    pipe->extra_sz = strtol (param1.ptr, NULL, 16);
    if (master.debug & DEBUG_CHUNK) printf ("chunk of size %ld found\n", pipe->extra_sz);
    if (pipe->extra_sz == 0 && param1.len > 0) {
      int ret = pipe->read_callback (pipe, buffer, 0, 1);
      pipe->in.flags |= HIN_DONE;
      return 1;
    }
  }
  return 0;
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
    hin_buffer_t * new = hin_pipe_get_buffer (pipe, READ_SZ);
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
      if (master.debug & DEBUG_DEFLATE) printf ("deflate write to pipe write %d total %d%s\n", have, new->count, flush ? " flush" : "");
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

  if (master.debug & DEBUG_RW) printf ("chunked num %d flush %d\n", num, flush);

  hin_buffer_t * buf = malloc (sizeof *buf + num + 50);
  memset (buf, 0, sizeof (*buf));
  buf->parent = (void*)pipe;
  buf->flags = 0;
  buf->ptr = buf->buffer;
  buf->count = 0;
  buf->sz = num + 50;
  buf->ssl = pipe->out.ssl;

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
}

int httpd_pipe_upload_chunked (httpd_client_t * http, hin_pipe_t * pipe) {
  if (http->disable & HIN_HTTP_CHUNKUP) return 0;
  if (http->peer_flags & HIN_HTTP_CHUNKUP) {
    pipe->decode_callback = hin_pipe_decode_chunked;
    pipe->read_callback = hin_pipe_copy_chunked;
  }
  return 0;
}

