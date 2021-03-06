
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hin.h"
#include "hin_internal.h"
#include "conf.h"

static hin_buffer_t * new_buffer (hin_buffer_t * buffer, int min_sz) {
  if (buffer->debug & DEBUG_RW)
    hin_debug ("header requires more buffer\n");
  int sz = READ_SZ > min_sz ? READ_SZ : min_sz;
  hin_buffer_t * buf = calloc (1, sizeof (hin_buffer_t) + sz);
  buf->sz = sz;
  buf->fd = buffer->fd;
  buf->flags = buffer->flags;
  buf->callback = buffer->callback;
  buf->parent = buffer->parent;
  buf->ptr = buf->buffer;
  buf->ssl = buffer->ssl;
  buf->count = 0;
  basic_dlist_add_after (NULL, &buffer->list, &buf->list);
  buf->debug = buffer->debug;
  return buf;
}

int vheader (hin_buffer_t * buffer, const char * fmt, va_list ap) {
  if (buffer->list.next) {
    hin_buffer_t * next = hin_buffer_list_ptr (buffer->list.next);
    return vheader (next, fmt, ap);
  }
  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  va_list prev;
  va_copy (prev, ap);
  int len = vsnprintf (buffer->ptr + pos, sz, fmt, ap);
  if (len < 0) return 0;
  if (len > HIN_HTTPD_MAX_HEADER_LINE_SIZE) {
    hin_error ("header failed to write more");
    va_end (ap);
    return 0;
  }
  if (len >= sz) {
    hin_buffer_t * buf = new_buffer (buffer, len);
    return vheader (buf, fmt, prev);
  }
  buffer->count += len;
  return len;
}

int header (hin_buffer_t * buffer, const char * fmt, ...) {
  va_list ap;
  va_start (ap, fmt);

  int len = vheader (buffer, fmt, ap);

  va_end (ap);
  return len;
}

int header_raw (hin_buffer_t * buffer, const char * data, int len) {
  if (buffer->list.next) {
    hin_buffer_t * next = hin_buffer_list_ptr (buffer->list.next);
    return header_raw (next, data, len);
  }

  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  if (len > HIN_HTTPD_MAX_HEADER_LINE_SIZE) {
    hin_error ("header_raw failed to write more");
    return 0;
  }
  if (len > sz) {
    hin_buffer_t * buf = new_buffer (buffer, len);
    return header_raw (buf, data, len);
  }

  memcpy (buffer->ptr + pos, data, len);
  buffer->count += len;
  return len;
}

void * header_ptr (hin_buffer_t * buffer, int len) {
  if (buffer->list.next) {
    return header_ptr (hin_buffer_list_ptr (buffer->list.next), len);
  }

  int pos = buffer->count;
  int sz = buffer->sz - buffer->count;
  if (len > HIN_HTTPD_MAX_HEADER_LINE_SIZE) {
    hin_error ("header_ptr failed to write more");
    return NULL;
  }
  if (len > sz) {
    hin_buffer_t * buf = new_buffer (buffer, len);
    return header_ptr (buf, len);
  }

  char * ptr = buffer->ptr + pos;
  buffer->count += len;
  return ptr;
}

int header_date (hin_buffer_t * buf, const char * fmt, time_t time) {
  struct tm data;
  struct tm *info = gmtime_r (&time, &data);
  if (info == NULL) { hin_perror ("gmtime_r"); return 0; }
  int sz = 80;
  char * buffer = header_ptr (buf, sz);
  int len = strftime (buffer, sz, fmt, info);
  if (len <= 0) {
    len = 0;
  }
  buf->count -= (sz - len);
  return len;
}


