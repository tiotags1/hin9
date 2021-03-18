
#ifndef HIN_LUA_H
#define HIN_LUA_H

#include <lua5.1/lua.h>
#include <lua5.1/lualib.h>
#include <lua5.1/lauxlib.h>

typedef struct { const char * name; int (*ptr) (lua_State *); } lua_function_t;

typedef struct hin_server_data_struct {
  // callback
  int refcount;
  int request_callback;
  int error_callback;
  int finish_callback;
  uint32_t magic;
  uint32_t disable;
  int timeout;
  char * hostname;
  uint32_t debug;
  int cwd_fd;
  char * cwd_path;
  void * cwd_dir;
  lua_State *L;
  struct hin_server_data_struct * next;
} hin_server_data_t;

int lua_add_functions (lua_State * L, lua_function_t * func);

int run_file (lua_State * L, const char * path);
int run_function (lua_State * L, const char * name);

int hin_server_set_work_dir (hin_server_data_t * server, const char * rel_path);

#endif

