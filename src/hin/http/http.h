#ifndef HIN_HTTP_H
#define HIN_HTTP_H

#include <zlib.h>
#include <time.h>

#include "uri.h"

enum {
HIN_REQ_HEADERS = 0x1, HIN_REQ_DATA = 0x2, HIN_REQ_POST = 0x4, HIN_REQ_WAIT = 0x8,
HIN_REQ_PROXY = 0x10, HIN_REQ_CGI = 0x20, HIN_REQ_FCGI = 0x40, HIN_REQ_END = 0x80,
HIN_REQ_ENDING = 0x100, HIN_REQ_ERROR = 0x200, HIN_REQ_ERROR_HANDLED = 0x400
};

enum { HIN_HTTP_GET = 1, HIN_HTTP_POST, HIN_HTTP_HEAD };

enum { HIN_HTTP_KEEPALIVE = 0x1, HIN_HTTP_RANGE = 0x2, HIN_HTTP_MODIFIED = 0x4, HIN_HTTP_ETAG = 0x8,
HIN_HTTP_CACHE = 0x10, HIN_HTTP_CHUNKED = 0x20, HIN_HTTP_DEFLATE = 0x40, HIN_HTTP_DATE = 0x80,
HIN_HTTP_VER0 = 0x100, HIN_HTTP_BANNER = 0x200,
HIN_HTTP_CHUNKED_UPLOAD = 0x400, HIN_HTTP_LOCAL_CACHE = 0x800 };

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

  off_t pos, count;
  void * file;

  time_t cache;
  uint32_t cache_flags;
  time_t modified_since;
  uintptr_t cache_key1, cache_key2;
  uint64_t etag;

  int post_fd;
  off_t post_sz;
  char * post_sep;

  char * append_headers;
  char * content_type;
  char * hostname;

  hin_buffer_t * read_buffer;

  uint32_t debug;
  string_t headers;
  z_stream z;

  hin_timer_t timer;
  void * vhost;

  // TODO remove
  int file_fd;
  char * file_path;
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

  uint32_t debug;
  hin_buffer_t * read_buffer;
} http_client_t;

#define HIN_HTTP_PATH_ACCEPT "[%w_%-/%.%+%%&#$?=,;]+"

#include <basic_pattern.h>
#include <stdarg.h>

int vheader (hin_buffer_t * buffer, const char * fmt, va_list ap);
int header (hin_buffer_t * buffer, const char * fmt, ...);
int header_raw (hin_buffer_t * buffer, const char * data, int len);
void * header_ptr (hin_buffer_t * buffer, int len);
int header_date (hin_buffer_t * buffer, const char * name, time_t time);
int header_cache_control (hin_buffer_t * buf, uint32_t flags, time_t max_age);
int httpd_write_common_headers (httpd_client_t * http, hin_buffer_t * buf);

const char * http_status_name (int nr);
int httpd_parse_cache_str (const char * str, size_t len, uint32_t * flags_out, time_t * max_age);

int httpd_respond_text (httpd_client_t * http, int status, const char * body);
int httpd_respond_error (httpd_client_t * http, int status, const char * body);
int httpd_respond_fatal (httpd_client_t * http, int status, const char * body);
int httpd_respond_fatal_and_full (httpd_client_t * http, int status, const char * body);
int httpd_respond_redirect (httpd_client_t * http, int status, const char * location);
int httpd_respond_redirect_https (httpd_client_t * http);

hin_pipe_t * send_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));
hin_pipe_t * receive_file (hin_client_t * client, int filefd, off_t pos, off_t count, uint32_t flags, int (*extra) (hin_pipe_t *));

int hin_client_ssl_init (hin_client_t * client);
void hin_client_ssl_cleanup (hin_client_t * client);

int httpd_client_start_request (httpd_client_t * http);
int httpd_client_finish_request (httpd_client_t * http);
int httpd_client_shutdown (httpd_client_t * http);
int http_client_shutdown (http_client_t * http);

int hin_cgi (httpd_client_t * http, const char * exe_path, const char * root_path, const char * script_path, const char * path_info);
int hin_fastcgi (httpd_client_t * http, void * fcgi_group, const char * script_path, const char * path_info);

// download
hin_server_t * httpd_create (const char * addr, const char * port, const char * sock_type, void * ssl_ctx);
http_client_t * http_download (const char * url, const char * save_path, int (*read_callback) (hin_buffer_t * buffer, int num, int flush));

// filters
int httpd_request_chunked (httpd_client_t * http);

// cache
int hin_cache_save (void * store, hin_pipe_t * pipe);
int hin_cache_finish (httpd_client_t * client, hin_pipe_t * pipe);
int hin_cache_check (void * store, httpd_client_t * client);

#include "utils.h"

#endif

