
#ifndef HIN_VHOST_H
#define HIN_VHOST_H

#include "system/hin_lua.h"

enum {
HIN_HSTS_SUBDOMAINS = 0x1, HIN_HSTS_PRELOAD = 0x2,
HIN_HSTS_NO_REDIRECT = 0x4, HIN_HSTS_NO_HEADER = 0x8,
HIN_DIRECTORY_LISTING = 0x10, HIN_DIRECTORY_NO_REDIRECT = 0x20,
};

enum { HIN_VHOST_MAP_START = 1, HIN_VHOST_MAP_FINISH };

typedef struct hin_ssl_ctx_struct {
  int refcount;
  uint32_t magic;
  void * ctx;
  const char * cert;
  const char * key;
  struct hin_ssl_ctx_struct * next;
} hin_ssl_ctx_t;

typedef struct httpd_vhost_map_struct {
  int state;
  const char * pattern;
  struct httpd_vhost_struct * vhost;
  lua_State * lua;
  int callback;
  struct httpd_vhost_map_struct * next, * prev;
} httpd_vhost_map_t;

typedef struct httpd_vhost_struct {
  // callback
  int refcount;
  uint32_t magic;
  uint32_t disable;
  uint32_t debug;
  uint32_t vhost_flags;
  int timeout;
  char * hostname;
  int cwd_fd;
  void * cwd_dir;
  hin_ssl_ctx_t * ssl;
  void * ssl_ctx;
  httpd_vhost_map_t * map_start, * map_finish;
  int hsts;
  lua_State *L;
  struct httpd_vhost_struct * parent, * next;
} httpd_vhost_t;

int httpd_vhost_set_work_dir (httpd_vhost_t * vhost, const char * rel_path);

httpd_vhost_t * httpd_vhost_get (const char * name, int name_len);
int httpd_vhost_add (const char * name, int name_len, httpd_vhost_t * ptr);
void httpd_vhost_set_debug (uint32_t debug);

#include "http.h"

int httpd_vhost_switch (httpd_client_t * http, httpd_vhost_t * vhost);
int httpd_vhost_request (httpd_client_t * http, const char * name, int len);

#endif

