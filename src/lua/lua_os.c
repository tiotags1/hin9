
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "hin.h"
#include "hin_lua.h"
#include "conf.h"

enum {EXEC_WAIT=0, EXEC_FORK, EXEC_SWITCH};

static int l_hin_exec (lua_State *L) {
  const char * path = NULL;
  int exec_mode = EXEC_WAIT;
  char ** argv = NULL;
  char ** envp = NULL;
  lua_pushstring (L, "path");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TSTRING) {
    path = lua_tostring (L, -1);
  }
  lua_pop (L, 1);

  lua_pushstring (L, "mode");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TSTRING) {
    const char * name = lua_tostring (L, -1);
    if (strcmp (name, "wait") == 0) {
      exec_mode = EXEC_WAIT;
    } else if (strcmp (name, "fork") == 0) {
      exec_mode = EXEC_FORK;
    } else if (strcmp (name, "switch") == 0) {
      exec_mode = EXEC_SWITCH;
    }
  }
  lua_pop (L, 1);

  lua_pushstring (L, "argv");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TTABLE) {
    size_t len = lua_rawlen (L, -1);
    argv = malloc ((len + 1) * sizeof (char *));
    for (int i = 1; i <= len; i++) {
      lua_rawgeti (L, -1, i);
      const char * arg = lua_tostring (L, -1);
      argv[i-1] = (char*)arg;
      lua_pop (L, 1);
    }
    argv[len] = NULL;
  }
  lua_pop (L, 1);

  if (exec_mode == EXEC_WAIT || exec_mode == EXEC_FORK) {
    int pid = fork ();
    if (pid < 0) {
      perror ("fork");
      return 0;
    }
    if (pid) {
      if (exec_mode == EXEC_WAIT) {
        int status = 0;
        int ret = waitpid (pid, &status, 0);
        if (ret < 0) perror ("waitpid");
        lua_pushnumber (L, status);
        return 1;
      }
      if (argv) free (argv);
      if (envp) free (envp);
      return 0;
    }
  }

  if (path == NULL) { printf ("exec path is null\n"); return -1; }
  if (argv == NULL) { argv = calloc (1, sizeof (void*)); }
  envp = calloc (1, sizeof (void*));

  execvpe (path, argv, envp);
  perror ("execvpe");
  exit (1);
}

static lua_function_t functs [] = {
{"exec",		l_hin_exec },
{NULL, NULL},
};

int hin_lua_os_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

