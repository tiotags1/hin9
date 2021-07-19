
#ifndef HIN_LUA_H
#define HIN_LUA_H

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

typedef struct { const char * name; int (*ptr) (lua_State *); } lua_function_t;

int lua_add_functions (lua_State * L, lua_function_t * func);

int run_file (lua_State * L, const char * path);
int run_function (lua_State * L, const char * name);

int hin_lua_rawlen (lua_State * L, int index);

#endif

