
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hin.h>
#include "http.h"
#include <basic_pattern.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int hin_post_field_headers (hin_client_t * client, string_t * source) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  string_t start = *source;
  string_t line, param1, param2;
  while (1) {
    if (find_line (source, &line) == 0) {
      *source = start;
      return 0;
    }
    if (line.len == 0) { return 1; }
    if (master.debug & DEBUG_POST) printf ("line '%.*s'\n", (int)line.len, line.ptr);
    if (match_string (&line, "Content%-Disposition: form%-data; name=\"([%w]+)\"", &param1) > 0) {
      if (master.debug & DEBUG_POST) printf ("  post name '%.*s'\n", (int)param1.len, param1.ptr);
      http->field.name = strndup (param1.ptr, param1.len);
      if (match_string (&line, "; filename=\"([%w%s%.%-%_]*)\"", &param2) > 0) {
        if (master.debug & DEBUG_POST) printf ("  file name '%.*s'\n", (int)param2.len, param2.ptr);
        http->field.file_name = strndup (param2.ptr, param2.len);
      }
    }
    if (match_string (&line, "Content%-Type: ([%w%-/]+)", &param1) > 0) {
      if (master.debug & DEBUG_POST) printf ("  post type '%.*s'\n", (int)param1.len, param1.ptr);
    }
  }
  return source->ptr - start.ptr;
}

int hin_post_field_sep (hin_client_t * client, string_t * source) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  char * start = source->ptr;
  char * ptr = source->ptr;
  char * end = ptr + source->len;
  char * iter = http->post_sep;
  char * begin = ptr;
  while (1) {
    if (*ptr != *iter) {
      iter = http->post_sep;
      begin = ptr - 1;
    } else {
      iter++;
      if (*iter == '\0') {
        ptr++;
        int len = ptr - start;
        source->ptr += len;
        source->len -= len;
        if (match_string (source, "%-%-") > 0) {}
        if (match_string (source, "\r\n") > 0) {}
        else if (match_string (source, "\n") > 0) {}
        else { printf ("error? in last char '%.*s'\n", source->len > 10 ? 10 : (int)source->len, source->ptr); }
        return begin - start;
      }
    }
    ptr++;
    if (ptr >= end) {
      // failed
      return -1;
    }
  }
}

int hin_post_eat_data (hin_client_t * client, httpd_client_field_t * field, string_t * source, int flush) {
  if (field->fd == 0) {
    field->fd = openat (AT_FDCWD, "/tmp/upload.txt", O_RDWR | O_CLOEXEC | O_CREAT | O_TRUNC, 0600);
    if (field->fd < 0) perror ("openat");
  }
  if (master.debug & DEBUG_POST) printf ("writing upload to %d len %ld flush %d\n", field->fd, source->len, flush);
  int ret = write (field->fd, source->ptr, source->len);
  if (ret < 0) perror ("write");

  if (flush) {
    close (field->fd);
    field->fd = 0;
    free (field->name);
    field->name = NULL;
  }
}

int httpd_parse_post (hin_client_t * client, string_t * source) {
  httpd_client_t * http = (httpd_client_t*)&client->extra;
  string_t start = *source;
  if (source->len > http->post_sz) source->len = http->post_sz;

  int used;
  string_t step = *source;
  httpd_client_field_t * field = &http->field;
  string_t local = *source;
  int total = 0;
  if (field->name == NULL) {
    used = hin_post_field_sep (client, source);
    if (used != 0) {
      if (field->fd != 0) close (field->fd);
      return 0;
    }
  }
  while (1) {
    if (field->name == NULL) {
      // check headers
      used = hin_post_field_headers (client, source);
      if (used < 0) return used;
      if (used == 0) return (uintptr_t)source->ptr - (uintptr_t)start.ptr;
    } else {
      // eat post till post_sep
      local = *source;
      used = hin_post_field_sep (client, source);
      if (used >= 0) {
        local.len = used;
        hin_post_eat_data (client, field, &local, 1);
      } else {
        int len = strlen (http->post_sep);
        if (len  > local.len) len = local.len;
        local.len -= len;
        hin_post_eat_data (client, field, &local, 0);
        source->ptr += source->len;
        source->len = 0;
        return (uintptr_t)source->ptr - (uintptr_t)start.ptr;
      }
    }
  }

  return (uintptr_t)source->ptr - (uintptr_t)start.ptr;
}

