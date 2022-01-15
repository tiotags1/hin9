
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"
#include "conf.h"

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

int download_progress (http_client_t * http, hin_pipe_t * pipe, int num, int flush) {
  hin_download_tracker_t * p = http->progress;
  if (p == NULL) {
    p = calloc (1, sizeof (*p));
    p->pipe = pipe;
    http->progress = p;
  }

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

  fprintf (stderr, "%.*s: %.1f%s/%.1f%s \t%.1f %s/sec%s",
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

static int state_callback (http_client_t * http, uint32_t state, uintptr_t data) {
  return 0;
}

static int read_callback (hin_pipe_t * pipe, hin_buffer_t * buf, int num, int flush) {
  if (num <= 0) return 1;
  buf->count = num;
  hin_pipe_append_raw (pipe, buf);

  http_client_t * http = pipe->parent;
  if (num == 0 && flush != 0 && http->sz) {
  }

  int download_progress (http_client_t * http, hin_pipe_t * pipe, int num, int flush);
  if (http->debug & DEBUG_PROGRESS) {
    download_progress (http, pipe, num, flush);
  }
  //if (flush) return 1; // already cleaned in the write done handler
  return 0;
}

http_client_t * http_download_raw (http_client_t * http, const char * url1) {
  if (http == NULL) {
    http = http_connection_get (url1);
  }
  if (HIN_HTTPD_PROXY_CONNECTION_REUSE) {
    http->flags |= HIN_HTTP_KEEPALIVE;
  }

  http->debug = master.debug;

  http->read_callback = read_callback;
  http->state_callback = state_callback;

  http_connection_start (http);

  return http;
}


