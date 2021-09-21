
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

int hin_http_state (http_client_t * http, int state) {
  if (http->debug & DEBUG_HTTP)
    printf ("http %d state is %d\n", http->c.sockfd, state);
  if (http->state_callback) {
    http->state_callback (http, state);
  }
  return 0;
}

void http_client_unlink (http_client_t * http);

static int connected (hin_buffer_t * buffer, int ret) {
  http_client_t * http = (http_client_t*)buffer->parent;

  master.num_connection++;
  int (*finish_callback) (http_client_t * http, int ret) = (void*)http->read_buffer;
  http->read_buffer = NULL;

  http->c.sockfd = ret;

  if (ret < 0) {
    finish_callback (http, ret);
    http_client_unlink (http);
    return 0;
  }

  if (http->uri.https) {
    if (hin_ssl_connect_init (&http->c) < 0) {
      fprintf (stderr, "ssl connect error %s:%s\n", http->host, http->port);
      finish_callback (http, -EPROTO);
      http_client_unlink (http);
      return 0;
    }
  }

  hin_buffer_t * buf = hin_lines_create_raw (READ_SZ);
  buf->flags = HIN_SOCKET | (http->c.flags & HIN_SSL);
  buf->fd = http->c.sockfd;
  buf->parent = http;
  buf->ssl = &http->c.ssl;
  buf->debug = http->debug;

  http->read_buffer = buf;
  if (hin_request_read (buf) < 0) {
    return 0;
  }

  finish_callback (http, http->c.sockfd);
  return 0;
}

static int state_callback (http_client_t * http, uint32_t state) {
  string_t url = http->uri.all;
  switch (state) {
  case HIN_HTTP_STATE_CONNECTED: // fall-through
  case HIN_HTTP_STATE_HEADERS: // fall-through
  case HIN_HTTP_STATE_FINISH:
  break;
  case HIN_HTTP_STATE_CONNECTION_FAILED:
    fprintf (stderr, "%.*s connection failed\n", (int)url.len, url.ptr);
  break;
  case HIN_HTTP_STATE_HEADERS_FAILED:
    fprintf (stderr, "%.*s failed to download headers\n", (int)url.len, url.ptr);
  break;
  case HIN_HTTP_STATE_ERROR:
    fprintf (stderr, "%.*s generic error\n", (int)url.len, url.ptr);
  break;
  default:
    fprintf (stderr, "%.*s error state is %d\n", (int)url.len, url.ptr, state);
  break;
  }
  return 0;
}

typedef struct {
  hin_pipe_t * pipe;
  time_t last;
  off_t last_sz;
} hin_download_tracker_t;

static int to_human_bytes (off_t amount, double * out, char ** unit) {
  *unit = "B";
  *out = amount;
  if (amount < 1024) goto end;
  *unit = "KB";
  *out = amount / 1024.0;
  amount /= 1024;
  if (amount < 1024) goto end;
  *unit = "MB";
  *out = amount / 1024.0;
  amount /= 1024;
  if (amount < 1024) goto end;
  *unit = "GB";
  *out = amount / 1024.0;
  amount /= 1024;
  if (amount < 1024) goto end;
  *unit = "TB";
end:
  return 0;
}

static int download_progress (hin_download_tracker_t * p, int num, int flush) {
  hin_pipe_t * pipe = p->pipe;
  http_client_t * http = pipe->parent;
  p->last_sz += num;
  if ((time (NULL) == p->last) && (flush == 0)) return 0;
  int single = 0;
  p->last = time (NULL);
  if (single) {
    printf ("\r");
  }
  char * u1, * u2, * u3;
  double s1, s2, s3;
  to_human_bytes (pipe->out.pos + num, &s1, &u1);
  to_human_bytes (http->sz, &s2, &u2);
  to_human_bytes (p->last_sz, &s3, &u3);

  printf ("%.*s: %.1f%s/%.1f%s \t%.1f %s/sec%s",
	(int)http->uri.all.len, http->uri.all.ptr,
	s1, u1,
	s2, u2,
	s3, u3,
	flush ? " finished" : "");
  p->last_sz = 0;
  if (!single || flush) {
    printf ("\n");
  }
  if (flush) {
    free (p);
    http->progress = NULL;
  }
  return 0;
}

static int read_callback (hin_pipe_t * pipe, hin_buffer_t * buf, int num, int flush) {
  if (num <= 0) return 1;
  buf->count = num;
  hin_pipe_write (pipe, buf);

  http_client_t * http = pipe->parent;
  if (num == 0 && flush != 0 && http->sz) {
  }

  if (http->debug & DEBUG_PROGRESS) {
    hin_download_tracker_t * progress = http->progress;
    if (progress == NULL) {
      progress = calloc (1, sizeof (*progress));
      progress->pipe = pipe;
      http->progress = progress;
    }
    download_progress (progress, num, flush);
  }
  //if (flush) return 1; // already cleaned in the write done handler
  return 0;
}

http_client_t * http_download_raw (http_client_t * http, const char * url1) {
  hin_uri_t info;
  char * url = strdup (url1);
  if (hin_parse_uri (url, 0, &info) < 0) {
    printf ("can't parse uri '%s'\n", url1);
    free (url);
    return NULL;
  }

  if (http == NULL) {
    http = calloc (1, sizeof (*http));
    http->debug = master.debug;
  }
  http->uri = info;

  if (http->host) free (http->host);
  if (http->port) free (http->port);
  http->host = strndup (info.host.ptr, info.host.len);
  if (info.port.len > 0) {
    http->port = strndup (info.port.ptr, info.port.len);
  } else {
    http->port = strdup ("80");
  }

  http->c.sockfd = -1;
  http->c.magic = HIN_CONNECT_MAGIC;
  http->c.ai_addrlen = sizeof (http->c.ai_addr);

  int http_client_start_headers (http_client_t * http, int ret);
  http->read_buffer = (hin_buffer_t*)http_client_start_headers;

  http->read_callback = read_callback;
  http->state_callback = state_callback;

  hin_connect (http->host, http->port, &connected, http, &http->c.ai_addr, &http->c.ai_addrlen);

  return http;
}



