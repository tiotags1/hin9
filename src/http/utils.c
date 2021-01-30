
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <basic_pattern.h>

#include "hin.h"
#include "http.h"

const char * http_status_name (int nr) {
  switch (nr) {
  case 100: return "Continue";
  case 200: return "OK";
  case 204: return "No Content";
  case 206: return "Partial Content";
  case 300: return "Multiple Choice";
  case 301: return "Moved Permanently";
  case 302: return "Found";
  case 304: return "Not Modified";
  case 307: return "Temporary Redirect";
  case 308: return "Permanent Redirect";
  case 400: return "Bad Request";
  case 401: return "Unauthorized";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 408: return "Request Timeout";
  case 411: return "Length Required";
  case 413: return "Payload Too Large";
  case 416: return "Range Not Satisfiable";
  case 500: return "Server error";
  case 501: return "Not Implemented";
  case 502: return "Bad Gateway";
  case 503: return "Service Unavailable";
  default: return "Unknown";
  }
}

int find_line (string_t * source, string_t * line) {
  char * ptr = source->ptr;
  char * max = ptr + source->len;
  char * last = ptr;
  line->ptr = ptr;
  line->len = 0;
  for (;ptr <= max; ptr++) {
    if (*ptr == '\n') {
      line->ptr = source->ptr;
      line->len = last - source->ptr;
      ptr++;
      source->len = max - ptr;
      source->ptr = ptr;
      return 1;
    } else if (*ptr == '\r') {
      last = ptr;
    } else {
      last = ptr + 1;
    }
  }
  return 0;
}

int hin_parse_uri (const char * url, int len, hin_uri_t * info) {
  if (len <= 0) len = strlen (url);
  string_t c;
  memset (info, 0, sizeof (*info));

  c.ptr = (char*)url;
  c.len = len;
  int err = 0;

  if (match_string (&c, "(https)://", &info->scheme) > 0) {
    info->https = 1;
  } else if (match_string (&c, "(http)://", &info->scheme) > 0) {
    info->https = 0;
  } else if (match_string (&c, "([%w%+%-%.]+)://", &info->scheme) > 0) {
  } else {
  }// generic scheme is letter+number, +, -, .
  if (match_string (&c, "([%w%.%-_]+)", &info->host) < 0) {
    //return -1;
    memset (&info->host, 0, sizeof (string_t));
  }
  if (match_string (&c, ":(%d+)", &info->port) > 0) {}
  if ((err = match_string (&c, "([%w%.=/;-_~!$&'%(%)%*%+,:@%%]+)", &info->path)) < 0) {
    printf ("error no path\n");
    return -1;
  }
  if (match_string (&c, "%?([%w%.=/;-_~!$&'%(%)%*%+,:@%%]+)", &info->query) > 0) {}
  if (match_string (&c, "%#([%w%.=/;-_~!$&'%(%)%*%+,:@%%]+)", &info->fragment) > 0) {}
  int used = (uintptr_t)c.ptr - (uintptr_t)url;
  info->all.ptr = (char*)url;
  info->all.len = used;
  //*uri = c;
  if (0) {
    printf ("url '%.*s'\nhost '%.*s'\nport '%.*s'\npath '%.*s'\nquery '%.*s'\nfragment '%.*s'\nhttps %d\n",
    len, url,
    (int)info->host.len, info->host.ptr,
    (int)info->port.len, info->port.ptr,
    (int)info->path.len, info->path.ptr,
    (int)info->query.len, info->query.ptr,
    (int)info->fragment.len, info->fragment.ptr,
    info->https
    );
  }

  return used;
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

int hin_client_deflate_init (httpd_client_t * http) {
  http->z.zalloc = Z_NULL;
  http->z.zfree = Z_NULL;
  http->z.opaque = Z_NULL;
  int ret = deflateInit (&http->z, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) {
    printf ("deflate init failed\n");
    return -1;
  }
  http->peer_flags |= HIN_HTTP_DEFLATE;
  httpd_request_chunked (http);
  return 0;
}

