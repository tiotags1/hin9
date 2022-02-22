
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <basic_pattern.h>

#include "hin.h"
#include "http.h"

const char * http_status_name (int nr) {
  switch (nr) {
  case 100: return "Continue";
  case 101: return "Switching Protocols";
  case 102: return "Processing";
  case 103: return "Early Hints";
  case 200: return "OK";
  case 201: return "Created";
  case 202: return "Accepted";
  case 203: return "Non-Authoritative Information";
  case 204: return "No Content";
  case 205: return "Reset Content";
  case 206: return "Partial Content";
  case 207: return "Multi-Status";
  case 208: return "Already Reported";
  case 300: return "Multiple Choice";
  case 301: return "Moved Permanently";
  case 302: return "Found";
  case 303: return "See Other";
  case 304: return "Not Modified";
  case 305: return "Use Proxy";
  case 306: return "Switch Proxy";
  case 307: return "Temporary Redirect";
  case 308: return "Permanent Redirect";
  case 400: return "Bad Request";
  case 401: return "Unauthorized";
  case 402: return "Payment Required";
  case 403: return "Forbidden";
  case 404: return "Not Found";
  case 405: return "Method Not Allowed";
  case 406: return "Not Acceptable";
  case 407: return "Proxy Authentication Required";
  case 408: return "Request Timeout";
  case 409: return "Conflict";
  case 410: return "Gone";
  case 411: return "Length Required";
  case 412: return "Precondition Failed";
  case 413: return "Payload Too Large";
  case 414: return "URI Too Long";
  case 415: return "Unsupported Media Type";
  case 416: return "Range Not Satisfiable";
  case 425: return "Too Early";
  case 426: return "Upgrade Required";
  case 428: return "Precondition Required";
  case 429: return "Too Many Requests";
  case 431: return "Request Header Fields Too Large";
  case 451: return "Unavailable For Legal Reasons";
  case 500: return "Server error";
  case 501: return "Not Implemented";
  case 502: return "Bad Gateway";
  case 503: return "Service Unavailable";
  case 504: return "Gateway Timeout";
  case 505: return "HTTP Version Not Supported";
  case 511: return "Network Authentication Required";

  default: return "Unknown";
  }
}

const char * hin_http_method_name (int num) {
  if (num == HIN_METHOD_GET) {
    return "GET";
  } else if (num == HIN_METHOD_POST) {
    return "POST";
  } else if (num == HIN_METHOD_HEAD) {
    return "HEAD";
  }
  return NULL;
}

int hin_http_parse_header_line (string_t * line, int * method, string_t * path, int * version) {
  string_t methods, paths, versions;
  if (match_string (line, "(%a+) ("HIN_HTTP_PATH_ACCEPT") HTTP/([%d%.]+)", &methods, &paths, &versions) <= 0) return -1;

  if (method) {
    if (matchi_string_equal (&methods, "GET") > 0) {
      *method = HIN_METHOD_GET;
    } else if (matchi_string_equal (&methods, "POST") > 0) {
      *method = HIN_METHOD_POST;
    } else if (matchi_string_equal (&methods, "HEAD") > 0) {
      *method = HIN_METHOD_HEAD;
    } else {
      *method = 0;
    }
  }

  if (version) {
    if (match_string_equal (&versions, "1.1") > 0) {
      *version = 0x11;
    } else if (match_string_equal (&versions, "1.0") > 0) {
      *version = 0x10;
    } else {
      *version = 0;
    }
  }

  if (path) {
    *path = paths;
  }

  return 0;
}

static unsigned long int my_strtoul (const char* str, const char** endptr, int base) {
  int ch;
  const char * ptr = str;
  unsigned long num = 0;
  while (1) {
    ch = *ptr;
    if (ch >= '0' && ch <= '9') ch = ch - '0';
    else if (ch >= 'A' && ch <= 'Z') ch = 10 + ch - 'A';
    else break;
    if (ch >= base) break;
    ptr++;
    num = num * base + ch;
  }
  if (endptr) *endptr = ptr;
  return num;
}

char * hin_parse_url_encoding (string_t * source, uint32_t flags) {
  const char * p1 = source->ptr;
  const char * max = source->ptr + source->len;

  char * new = malloc (source->len + 1);
  char * p2 = new;

  while (1) {
    if (p1 >= max) break;
    if (*p1 == '%') {
      p1++;
      int utf = my_strtoul (p1, &p1, 16);
      if (utf > 0x1F)
        *p2 = utf;
    } else {
      *p2 = *p1;
      p1++;
    }
    p2++;
  }
  *p2 = '\0';

  //printf ("old '%.*s'\n", source->len, source->ptr);
  //printf ("new '%s'\n", new);

  return new;
}

int hin_find_line (string_t * source, string_t * line) {
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

#define PATH_ACCEPT "%w.=/;-_~!$&'%(%)%*%+,:%%@"

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
  if ((err = match_string (&c, "(["PATH_ACCEPT"]+)", &info->path)) < 0) {
    printf ("error no path\n");
    return -1;
  }
  if (match_string (&c, "%?(["PATH_ACCEPT"]+)", &info->query) > 0) {}
  if (match_string (&c, "%#(["PATH_ACCEPT"]+)", &info->fragment) > 0) {}
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

int httpd_request_chunked (httpd_client_t * http);

int hin_client_deflate_init (httpd_client_t * http) {
  http->z.zalloc = Z_NULL;
  http->z.zfree = Z_NULL;
  http->z.opaque = Z_NULL;
  int windowsBits = 15;
  if (http->peer_flags & HIN_HTTP_DEFLATE) {
    http->peer_flags = (http->peer_flags & ~HIN_HTTP_COMPRESS) | HIN_HTTP_DEFLATE;
  } else if (http->peer_flags & HIN_HTTP_GZIP) {
    http->peer_flags = (http->peer_flags & ~HIN_HTTP_COMPRESS) | HIN_HTTP_GZIP;
    windowsBits |= 16;
  } else {
    printf ("error! internal error useless zlib init\n");
    return -1;
  }
  int ret = deflateInit2 (&http->z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowsBits, 8, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    printf ("deflate init failed\n");
    return -1;
  }
  httpd_request_chunked (http);
  return 0;
}

