
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <basic_pattern.h>

#include "hin/hin.h"
#include "hin/http/http.h"

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

