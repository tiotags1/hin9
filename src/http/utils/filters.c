
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
    if (pipe->debug & DEBUG_HTTP_FILTER) printf ("  chunk sz left %lld\n", (long long)decode->chunk_sz);
    if (decode->chunk_sz > 0) {
      uintptr_t consume = decode->chunk_sz;
      if (consume > source.len) consume = source.len;
      if (pipe->debug & DEBUG_HTTP_FILTER) printf ("  chunk consume %lld\n", (long long)consume);
      hin_buffer_t * buf = hin_pipe_get_buffer (pipe, consume);
      memcpy (buf->ptr, source.ptr, consume);
      if (pipe->read_callback (pipe, buf, consume, 0))
        hin_buffer_clean (buf);
      if (decode->chunk_sz > (off_t)consume) {
        // want more;
        decode->chunk_sz -= consume;
        return 1;
      }
      source.ptr += consume;
      source.len -= consume;
      decode->chunk_sz -= consume;
      if (source.len < 2) {
        hin_pipe_decode_prepare_half_read (pipe, buffer, source.len-2, num);
        return 1;
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
        return 1;
      }
      printf ("chunk decode couldn't find in '%.*s'\n", (int)(source.len > 20 ? 20 : source.len), source.ptr);
      return -1;
    }
    decode->chunk_sz = strtol (param1.ptr, NULL, 16);
    if (pipe->debug & DEBUG_HTTP_FILTER) printf ("  chunk sz %lld found\n", (long long)decode->chunk_sz);
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
  hin_client_t * c = (hin_client_t*)pipe->parent;
  z_stream * z = NULL;
  uint32_t peer_flags = 0;
  httpd_client_t * http = (httpd_client_t*)pipe->parent;
  if (c->magic == HIN_CONNECT_MAGIC) {
    http = c->parent;
  }
  z = &http->z;
  peer_flags = http->peer_flags;

  int have;
  z->avail_in = num;
  z->next_in = (Bytef *)buffer->ptr;
  buffer->count = num;

  if (pipe->debug & DEBUG_HTTP_FILTER)
    printf ("http(d) %d deflate num %d flush %d\n", c->sockfd, num, flush);
  char numbuf[10]; // size of max nr (7 bytes) + crlf + \0

  do {
    hin_buffer_t * new = hin_pipe_get_buffer (pipe, READ_SZ);
    new->count = 0;
    if (peer_flags & HIN_HTTP_CHUNKED) {
      new->sz -= (sizeof (numbuf) + 8); // crlf + 0+crlfcrlf + \0
      new->count = sizeof (numbuf);
    }
    z->avail_out = new->sz;
    z->next_out = (Bytef *)&new->buffer[new->count];
    deflate (z, flush ? Z_FINISH : Z_NO_FLUSH);
    have = new->sz - z->avail_out;

    if (have > 0) {
      new->count += have;
      new->fd = pipe->out.fd;

      if (peer_flags & HIN_HTTP_CHUNKED) {
        new->sz += sizeof (numbuf) + 8;
        header (new, "\r\n");
        if (flush && (z->avail_out != 0)) {
          header (new, "0\r\n\r\n");
        }

        int num = snprintf (numbuf, sizeof (numbuf), "%x\r\n", have);
        if (num < 0 || num >= (int)sizeof (numbuf)) { printf ("weird error\n"); }
        int offset = sizeof (numbuf) - num;
        char * ptr = new->buffer + offset;
        memcpy (ptr, numbuf, num);
        new->ptr = ptr;
        new->count -= offset;
      }
      if (pipe->debug & DEBUG_HTTP_FILTER)
        printf ("  deflate write %d total %d %s\n", have, new->count, flush ? "flush" : "cont");
      hin_pipe_append_raw (pipe, new);
    } else {
      hin_buffer_clean (new);
    }
  } while (z->avail_out == 0);
  return 1;
}

int hin_pipe_copy_chunked (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush) {
  hin_client_t * client = (hin_client_t*)pipe->parent;

  if (buffer->debug & DEBUG_HTTP_FILTER)
    printf ("http(d) %d chunked num %d flush %d\n", client->sockfd, num, flush);

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

  hin_pipe_append_raw (pipe, buf);

  return 1;
}

int httpd_request_chunked (httpd_client_t * http) {
  if (http->peer_flags & HIN_HTTP_VER0) {
    http->peer_flags &= ~(HIN_HTTP_KEEPALIVE | HIN_HTTP_CHUNKED);
  } else {
    if (http->peer_flags & HIN_HTTP_KEEPALIVE) {
      http->peer_flags |= HIN_HTTP_CHUNKED;
    }
    return 1;
  }
  return 0;
}

int httpd_pipe_set_chunked (httpd_client_t * http, hin_pipe_t * pipe) {
  if (http->peer_flags & HIN_HTTP_COMPRESS) {
    httpd_request_chunked (http);
    pipe->read_callback = hin_pipe_copy_deflate;
  } else if ((http->peer_flags & HIN_HTTP_CHUNKED)) {
    if (httpd_request_chunked (http)) {
      pipe->read_callback = hin_pipe_copy_chunked;
    }
  }
  return 0;
}

static int httpd_pipe_error_callback (hin_pipe_t * pipe, int err) {
  printf ("http %d error!\n", pipe->out.fd);
  httpd_client_shutdown (pipe->parent);
  return 0;
}

int httpd_pipe_set_http11_response_options (httpd_client_t * http, hin_pipe_t * pipe) {
  pipe->out.fd = http->c.sockfd;
  pipe->out.flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  pipe->out.ssl = &http->c.ssl;
  pipe->out.pos = 0;
  pipe->parent = http;
  pipe->debug = http->debug;
  pipe->out_error_callback = httpd_pipe_error_callback;

  pipe->left = pipe->sz = http->count;

  if (http->status == 304 || http->method == HIN_METHOD_HEAD) {
    http->peer_flags &= ~(HIN_HTTP_CHUNKED | HIN_HTTP_COMPRESS);
    pipe->left = pipe->sz = 0;
  } else {
    httpd_pipe_set_chunked (http, pipe);
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

