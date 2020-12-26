#ifndef HIN_URI_H
#define HIN_URI_H

#include <basic_pattern.h>

// url

typedef struct {
  string_t all;
  string_t scheme, host, port;
  string_t path, query, fragment;
  int https;
} hin_uri_t;

int hin_parse_uri (const char * url, int len, hin_uri_t * info);

#endif

