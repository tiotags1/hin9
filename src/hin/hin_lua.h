
#ifndef HIN_LUA_H
#define HIN_LUA_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct { const char * name; int (*ptr) (lua_State *); } lua_function_t;

typedef struct hin_ssl_ctx_struct {
  int refcount;
  uint32_t magic;
  void * ctx;
  const char * cert;
  const char * key;
  struct hin_ssl_ctx_struct * next;
} hin_ssl_ctx_t;

typedef struct hin_server_data_struct {
  // callback
  int refcount;
  int request_callback;
  int error_callback;
  int finish_callback;
  uint32_t magic;
  uint32_t disable;
  uint32_t debug;
  int timeout;
  char * hostname;
  int cwd_fd;
  void * cwd_dir;
  hin_ssl_ctx_t * ssl;
  void * ssl_ctx;
  lua_State *L;
  struct hin_server_data_struct * parent, * next;
} hin_vhost_t;

int lua_add_functions (lua_State * L, lua_function_t * func);

int run_file (lua_State * L, const char * path);
int run_function (lua_State * L, const char * name);

int hin_server_set_work_dir (hin_vhost_t * vhost, const char * rel_path);

int hin_lua_rawlen (lua_State * L, int index);


hin_vhost_t * hin_vhost_get (const char * name, int name_len);
int hin_vhost_add (const char * name, int name_len, hin_vhost_t * ptr);

#include "http.h"

int httpd_vhost_switch (httpd_client_t * http, hin_vhost_t * vhost);
int httpd_vhost_request (httpd_client_t * http, const char * name, int len);

#endif

