
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libgen.h> // for dirname

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

int hin_lua_rawlen (lua_State * L, int index) {
  #if LUA_VERSION_NUM > 501
  return lua_rawlen (L, index);
  #else
  return lua_objlen (L, index);
  #endif
}

static int l_hin_dofile (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  if (path == NULL) { printf ("can't load nil file\n"); return 0; }
  if (*path == '/') {
    run_file (L, path);
    return 0;
  }
  char * conf_path = strdup (master.conf_path);
  const char * dir_path = dirname (conf_path);
  char * new = NULL;
  int ret = asprintf (&new, "%s/%s", dir_path, path);
  if (ret < 0) { perror ("asprintf"); return 0; }
  run_file (L, new);
  free (conf_path);
  free (new);
  return 0;
}

static lua_function_t functs [] = {
{"dofile",		l_hin_dofile },
{NULL, NULL},
};

int hin_lua_utils_init (lua_State * L) {
  return lua_add_functions (L, functs);
}


