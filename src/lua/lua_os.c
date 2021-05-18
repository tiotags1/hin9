
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
#include "worker.h"

enum {EXEC_WAIT=0, EXEC_FORK, EXEC_SWITCH};

typedef struct {
  hin_child_t c;
  lua_State * L;
  int callback_ref;
} hin_child1_t;

static int hin_child_callback (hin_child_t * child1, int status) {
  hin_child1_t * child = (hin_child1_t*)child1;
  lua_State * L = child->L;
  lua_rawgeti (L, LUA_REGISTRYINDEX, child->callback_ref);

  int num = 1;
  const char * error = NULL;
  if (WIFSIGNALED (status)) {
    if (WTERMSIG (status) == SIGSEGV) {
      error = "sigsegv";
    } else {
      error = "unnamed signal";
    }
  }

  lua_pushnumber (L, status);
  if (error) {
    lua_pushstring (L, error);
    num++;
  }

  if (lua_pcall (L, num, LUA_MULTRET, 0) != 0) {
    printf ("error running child close callback '%s'\n", lua_tostring (L, -1));
    return -1;
  }

  luaL_unref (L, LUA_REGISTRYINDEX, child->callback_ref);
  return 0;
}

static int hin_child_call_callback (hin_child1_t * child, int ret) {
  child->c.callback ((hin_child_t*)child, ret);
  free (child);
  return 0;
}

static int l_hin_exec (lua_State *L) {
  const char * path = NULL;
  int exec_mode = EXEC_WAIT;
  char ** argv = NULL;
  char ** envp = NULL;
  hin_child1_t * child = NULL;

  if (lua_type (L, 1) != LUA_TTABLE) {
    printf ("exec needs a table\n");
    return 0;
  }

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
    size_t len = hin_lua_rawlen (L, -1);
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

  lua_pushstring (L, "callback");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TFUNCTION) {
    child = calloc (1, sizeof (*child));
    child->L = L;
    child->callback_ref = luaL_ref (L, LUA_REGISTRYINDEX);
  }
  lua_pop (L, 1);

  if (exec_mode == EXEC_WAIT || exec_mode == EXEC_FORK) {
    int pid = fork ();
    if (pid < 0) {
      perror ("fork");
      return 0;
    }
    if (pid) {
      if (child) {
        child->c.pid = pid;
        child->c.callback = hin_child_callback;
        hin_children_add ((hin_child_t*)child);
      }
      if (argv) free (argv);
      if (envp) free (envp);
      if (exec_mode == EXEC_WAIT) {
        int status = 0;
        int ret = waitpid (pid, &status, 0);
        if (ret < 0) perror ("waitpid");
        if (child) {
          hin_child_call_callback (child, status);
        }
        lua_pushnumber (L, status);
        return 1;
      }
      return 0;
    }
  }

  if (path == NULL) { printf ("exec path is null\n"); return -1; }
  if (argv == NULL) {
    argv = calloc (2, sizeof (void*));
    argv[0] = (char*)path;
  }
  if (envp == NULL) {
    envp = (char**)master.envp;
  }

  execvpe (path, argv, envp);
  printf ("execvpe '%s' error: %s\n", path, strerror (errno));
  exit (1);
}

static lua_function_t functs [] = {
{"exec",		l_hin_exec },
{NULL, NULL},
};

int hin_lua_os_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

