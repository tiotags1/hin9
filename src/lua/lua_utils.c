
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libgen.h> // for dirname

#include "hin.h"

#include "system/hin_lua.h"

#include <basic_vfs.h>

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
    return -1;
  }

  err = lua_pcall (L, 0, 0, 0);
  if (err) {
    printf ("error! run_file '%s':'%s'\n", path, lua_tostring (L, -1));
    lua_pop (L, 1);
    return -1;
  }
  return 0;
}

int hin_lua_run_string (lua_State * L, const char * data, int len, const char * name) {
  int err;
  err = luaL_loadbuffer (L, data, len, name);
  if (err) {
    return -1;
  }

  err = lua_pcall (L, 0, 0, 0);
  if (err) {
    printf ("error! run_string '%s':'%s'\n", name, lua_tostring (L, -1));
    lua_pop (L, 1);
    return -1;
  }
  return 0;
}

int run_function (lua_State * L, const char * name) {
  lua_getglobal(L, name);

  if (lua_pcall (L, 0, 0, 0) != 0) {
    printf ("error! run_function '%s':'%s'\n", name, lua_tostring (L, -1));
    lua_pop (L, 1);
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

static int l_hin_require (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  int ret = 0;
  if (path == NULL) { lua_pushstring (L, "path nil"); return 1; }
  if (*path == '/') {
    ret = run_file (L, path);
    goto finish;
  }
  char * conf_path = strdup (master.conf_path);
  const char * dir_path = dirname (conf_path);
  char * new = NULL;
  ret = asprintf (&new, "%s/%s", dir_path, path);
  if (ret < 0) { perror ("asprintf"); return 0; }
  if (master.debug & DEBUG_CONFIG)
    printf ("lua require '%s'\n", new);
  ret = run_file (L, new);
  free (conf_path);
  free (new);
finish:
  if (ret < 0) {
    const char * reason = lua_tostring (L, -1);
    return luaL_error (L, "error! %s\n", reason);
  }
  return 0;
}

static int l_hin_include (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  int ret = 0;
  if (path == NULL) { lua_pushstring (L, "path nil"); return 1; }
  if (*path == '/') {
    ret = run_file (L, path);
    goto finish;
  }
  char * conf_path = strdup (master.conf_path);
  const char * dir_path = dirname (conf_path);
  char * new = NULL;
  ret = asprintf (&new, "%s/%s", dir_path, path);
  if (ret < 0) { perror ("asprintf"); return 0; }
  if (master.debug & DEBUG_CONFIG)
    printf ("lua include '%s'\n", new);
  ret = run_file (L, new);
  free (conf_path);
  free (new);
finish:
  if (ret < 0) {
    return 1;
  }
  return 0;
}

#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef STATX_MTIME
#include <linux/stat.h>
#endif

static int l_hin_file_age (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  if (path == NULL) { printf ("can't load nil file\n"); return 0; }

  struct statx stat;

  if (statx (AT_FDCWD, path, 0, STATX_MTIME, &stat) < 0) {
    printf ("statx '%s': %s\n", path, strerror (errno));
    return 0;
  }

  time_t t = time (NULL);
  time_t new = t - stat.stx_mtime.tv_sec;

  lua_pushnumber (L, new);

  return 1;
}

static lua_function_t functs [] = {
{"include",		l_hin_include },
{"require",		l_hin_require },
{"file_age",		l_hin_file_age },
{NULL, NULL},
};

int hin_lua_utils_init (lua_State * L) {
  return lua_add_functions (L, functs);
}


