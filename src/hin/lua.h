
#ifndef HIN_LUA_H
#define HIN_LUA_H

#include <lua5.1/lua.h>
#include <lua5.1/lualib.h>
#include <lua5.1/lauxlib.h>

typedef struct { const char * name; int (*ptr) (lua_State *); } lua_function_t;

typedef struct hin_server_data_struct {
  // callback
  int refcount;
  int callback;
  uint32_t magic;
  uint32_t disable;
  int timeout;
  char * hostname;
  lua_State *L;
  struct hin_server_data_struct * next;
} hin_server_data_t;

int lua_add_functions (lua_State * L, lua_function_t * func);

int run_file (lua_State * L, const char * path);
int run_function (lua_State * L, const char * name);

#endif

