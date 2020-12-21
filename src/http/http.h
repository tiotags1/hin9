#ifndef HIN_HTTP_H
#define HIN_HTTP_H

#include <zlib.h>
#include <time.h>

#include "uri.h"

enum { HTTP_FORM_MULTIPART=1 };

enum { HIN_HEADERS = 0x1, HIN_POST = 0x2, HIN_SERVICE = 0x4, HIN_WAIT = 0x8, HIN_END = 0x10 };

enum { HIN_HTTP_KEEP = 0x1, HIN_HTTP_VER0 = 0x4, HIN_HTTP_CHUNKED = 0x8, HIN_HTTP_DEFLATE = 0x10, HIN_HTTP_CAN_DEFLATE = 0x20 };

enum { HIN_HTTP_GET = 1, HIN_HTTP_POST };

enum { HIN_DISABLE_KEEPALIVE = 0x1, HIN_DISABLE_RANGE = 0x2, HIN_DISABLE_MODIFIED_SINCE = 0x4, HIN_DISABLE_ETAG = 0x8,
HIN_DISABLE_CACHE = 0x10, HIN_DISABLE_POST = 0x20, HIN_DISABLE_CHUNKED = 0x40, HIN_DISABLE_DEFLATE = 0x80,
HIN_DISABLE_DATE = 0x100 };

typedef struct {
  char * name;
  char * file_name;
  off_t sz;
  int fd;
} httpd_client_field_t;

typedef struct {
  uint32_t state;
  uint32_t flags;
  uint32_t disable;

  int status;
  int method;

  int filefd;
  const char * file_path;
  off_t pos, count, sz;

  time_t cache;
  time_t modified_since;
  uint64_t etag;

  int post_fd;
  int post_type;
  off_t post_sz;
  char * post_sep;
  httpd_client_field_t field;

  string_t headers, rest;
  z_stream z;
} httpd_client_t;

typedef struct {
  char * save_path;
  hin_uri_t uri;
  off_t sz;
} http_client_t;

#define HIN_HTTP_PATH_ACCEPT "[%w_/%.%+%%&#$?=]+"

#include <basic_pattern.h>

int find_line (string_t * source, string_t * line);
int header (hin_client_t * client, hin_buffer_t * buffer, const char * fmt, ...);
const char * http_status_name (int nr);

int httpd_parse_req (hin_client_t * client, string_t * source);

int httpd_respond_error (hin_client_t * client, int status, const char * body);

int send_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));
int receive_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));

int hin_client_ssl_init (hin_client_t * client);
void hin_client_ssl_cleanup (hin_client_t * client);

int httpd_client_finish (hin_client_t * client);
int httpd_client_start_request (hin_client_t * client);
int httpd_client_finish_request (hin_client_t * client);
int httpd_client_finish_post (hin_client_t * client);

int hin_request_headers (hin_client_t * client);

// download
hin_client_t * http_download (const char * url, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush));


#endif

