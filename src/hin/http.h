#ifndef HIN_HTTP_H
#define HIN_HTTP_H

#include <zlib.h>
#include <time.h>

#include "uri.h"
#include <basic_timer.h>

enum { HIN_REQ_HEADERS = 0x1, HIN_REQ_DATA = 0x2, HIN_REQ_POST = 0x4, HIN_REQ_WAIT = 0x8,
	HIN_REQ_PROXY = 0x10, HIN_REQ_CGI = 0x20, HIN_REQ_END = 0x40, HIN_REQ_ENDING = 0x80,
	HIN_REQ_RAW = 0x100, HIN_REQ_ERROR_HANDLED = 0x200 };

enum { HIN_HTTP_GET = 1, HIN_HTTP_POST, HIN_HTTP_HEAD };

enum { HIN_HTTP_KEEPALIVE = 0x1, HIN_HTTP_RANGE = 0x2, HIN_HTTP_MODIFIED = 0x4, HIN_HTTP_ETAG = 0x8,
HIN_HTTP_CACHE = 0x10, HIN_HTTP_CHUNKED = 0x20, HIN_HTTP_DEFLATE = 0x40, HIN_HTTP_DATE = 0x80,
HIN_HTTP_VER0 = 0x100, HIN_HTTP_SERVNAME = 0x200, HIN_HTTP_CHUNKUP = 0x400 };

typedef struct {
  char * name;
  char * file_name;
  off_t sz;
  int fd;
} httpd_client_field_t;

typedef struct {
  hin_client_t c;
  uint32_t state;
  uint32_t peer_flags, disable;

  int status;
  int method;

  int file_fd;
  char * file_path;
  off_t pos, count;

  time_t cache;
  time_t modified_since;
  uint64_t etag;

  int post_fd;
  off_t post_sz;
  char * post_sep;

  char * append_headers;
  char * content_type;

  basic_time_t next_time;

  hin_buffer_t * read_buffer;

  string_t headers;
  z_stream z;
} httpd_client_t;

typedef struct {
  hin_client_t c;
  uint32_t flags;
  uint32_t io_state;
  hin_uri_t uri;
  char * host, * port;

  char * save_path;
  int save_fd;
  off_t sz;

  hin_buffer_t * read_buffer;
} http_client_t;

#define HIN_HTTP_PATH_ACCEPT "[%w_%-/%.%+%%&#$?=,;]+"

#include <basic_pattern.h>

int header (hin_client_t * client, hin_buffer_t * buffer, const char * fmt, ...);
int header_raw (hin_client_t * client, hin_buffer_t * buffer, const char * data, int len);
int header_date (hin_client_t * client, hin_buffer_t * buffer, const char * name, time_t time);
int httpd_write_common_headers (hin_client_t * client, hin_buffer_t * buf);

const char * http_status_name (int nr);

int httpd_parse_req (httpd_client_t * http, string_t * source);

int httpd_respond_text (httpd_client_t * http, int status, const char * body);
int httpd_respond_error (httpd_client_t * http, int status, const char * body);
int httpd_respond_fatal (httpd_client_t * http, int status, const char * body);

hin_pipe_t * send_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));
hin_pipe_t * receive_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));

int hin_client_ssl_init (hin_client_t * client);
void hin_client_ssl_cleanup (hin_client_t * client);

int httpd_client_start_request (httpd_client_t * http);
int httpd_client_finish_request (httpd_client_t * http);
int httpd_client_shutdown (httpd_client_t * http);
int http_client_shutdown (http_client_t * http);

//int hin_request_headers (hin_client_t * client);

// download
hin_client_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx);
http_client_t * http_download (const char * url, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush));

#include "utils.h"

#endif

