
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "hin_lua.h"

int lua_add_functions (lua_State * L, lua_function_t * func) {
  while (1) {
    if (func == NULL || func->name == NULL || func->ptr == NULL) break;
    lua_pushcfunction (L, func->ptr);
    lua_setglobal (L, func->name);
    func++;
  }
  return 0;
}

int run_file (lua_State * L, const char * path) {
  int err;
  err = luaL_loadfile (L, path);
  if (err) {
    fprintf (stderr, "Couldn't load file: %s\n", lua_tostring (L, -1));
    return -1;
  }

  err = lua_pcall (L, 0, LUA_MULTRET, 0);
  if (err) {
    fprintf (stderr, "Failed to run script1: %s\n", lua_tostring (L, -1));
    return -1;
  }
  return 0;
}

int run_function (lua_State * L, const char * name) {
  lua_getglobal(L, name);

  if (lua_pcall (L, 0, 0, 0) != 0) {
    fprintf (stderr, "Failed to run function '%s': %s\n", name, lua_tostring (L, -1));
    return -1;
  }
  return 0;
}


