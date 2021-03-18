
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "lua.h"

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


#include <fcntl.h>
#include <unistd.h>

int hin_server_set_work_dir (hin_server_data_t * server, const char * rel_path) {
  char * abs_path = realpath (rel_path, NULL);
  if (abs_path == NULL) { perror ("realpath"); return -1; }
  int fd = openat (AT_FDCWD, abs_path, O_DIRECTORY | O_CLOEXEC);
  if (server->debug & DEBUG_CONFIG) printf ("lua server cwd set to %s\n", abs_path);
  if (fd < 0) { perror ("cwd openat"); return -1; }
  if (server->cwd_path) { free (server->cwd_path); }
  if (server->cwd_fd && server->cwd_fd != AT_FDCWD) { close (server->cwd_fd); }
  server->cwd_path = abs_path;
  server->cwd_fd = fd;

  return 0;
}


