#ifndef HIN_HTTP_H
#define HIN_HTTP_H

#include <zlib.h>
#include <time.h>

#include "uri.h"
#include <basic_timer.h>

enum { HIN_REQ_HEADERS = 0x1, HIN_REQ_DATA = 0x2, HIN_REQ_POST = 0x4, HIN_REQ_WAIT = 0x8, HIN_REQ_END = 0x10 };

enum { HIN_HTTP_GET = 1, HIN_HTTP_POST };

enum { HIN_HTTP_KEEPALIVE = 0x1, HIN_HTTP_RANGE = 0x2, HIN_HTTP_MODIFIED = 0x4, HIN_HTTP_ETAG = 0x8,
HIN_HTTP_CACHE = 0x10, HIN_HTTP_CHUNKED = 0x20, HIN_HTTP_DEFLATE = 0x40, HIN_HTTP_DATE = 0x80,
HIN_HTTP_VER0 = 0x100 };

#define HTTPD_TIMEOUT 10

typedef struct {
  char * name;
  char * file_name;
  off_t sz;
  int fd;
} httpd_client_field_t;

typedef struct {
  uint32_t state;
  uint32_t peer_flags, disable;

  int status;
  int method;

  int filefd;
  const char * file_path;
  off_t pos, count;

  time_t cache;
  time_t modified_since;
  uint64_t etag;

  int post_fd;
  off_t post_sz;
  char * post_sep;

  char * append_headers;

  basic_time_t next_time;

  string_t headers;
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
int hin_string_equali (string_t * source, const char * format, ...);

int httpd_parse_req (hin_client_t * client, string_t * source);

int httpd_respond_error (hin_client_t * client, int status, const char * body);

hin_pipe_t * send_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));
hin_pipe_t * receive_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));

int hin_client_ssl_init (hin_client_t * client);
void hin_client_ssl_cleanup (hin_client_t * client);

int httpd_client_start_request (hin_client_t * client);
int httpd_client_finish_request (hin_client_t * client);
int httpd_client_shutdown (hin_client_t * client);

int hin_request_headers (hin_client_t * client);

// download
hin_client_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx);
hin_client_t * http_download (const char * url, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush));


#endif

