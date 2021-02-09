
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "lua.h"
#include "conf.h"

int hin_server_callback (hin_client_t * client) {
  if (client->parent == NULL) {
    printf ("no socket parent\n");
    return -1;
  }
  hin_client_t * server_client = (hin_client_t*)client->parent;
  hin_server_data_t * server = (hin_server_data_t*)server_client->parent;

  lua_State * L = server->L;
  lua_rawgeti (L, LUA_REGISTRYINDEX, server->request_callback);
  lua_pushlightuserdata (L, server);
  lua_pushlightuserdata (L, client);

  if (lua_pcall (L, 2, LUA_MULTRET, 0) != 0) {
    printf ("error running request callback '%s'\n", lua_tostring (L, -1));
    return -1;
  }
}

int hin_server_finish_callback (hin_client_t * client) {
  if (client->parent == NULL) {
    printf ("no socket parent\n");
    return -1;
  }
  hin_client_t * server_client = (hin_client_t*)client->parent;
  hin_server_data_t * server = (hin_server_data_t*)server_client->parent;

  lua_State * L = server->L;
  if (server->finish_callback == 0) return 0;
  lua_rawgeti (L, LUA_REGISTRYINDEX, server->finish_callback);
  lua_pushlightuserdata (L, server);
  lua_pushlightuserdata (L, client);

  if (lua_pcall (L, 2, LUA_MULTRET, 0) != 0) {
    printf ("error running request callback '%s'\n", lua_tostring (L, -1));
    return -1;
  }
  return 0;
}

static lua_State * internal_lua = NULL;

int hin_timeout_callback (float dt) {
  lua_State * L = internal_lua;

  lua_getglobal (L, "timeout_callback");
  if (lua_isnil (L, 1)) { return 0; }
  lua_pushnumber (L, 1);

  if (lua_pcall (L, 1, LUA_MULTRET, 0) != 0) {
    printf ("error running timeout callback '%s'\n", lua_tostring (L, -1));
    return -1;
  }

  return 0;
}

void lua_server_clean (hin_server_data_t * server) {
  lua_State * L = server->L;
  luaL_unref (L, LUA_REGISTRYINDEX, server->request_callback);
  if (server->finish_callback)
    luaL_unref (L, LUA_REGISTRYINDEX, server->finish_callback);

  if (server->hostname) free (server->hostname);
  free (server);
}

void hin_lua_clean () {
  hin_server_data_t * elem = master.servers;
  hin_server_data_t * next;
  for (;elem; elem = next) {
    next = elem->next;
    lua_server_clean (elem);
  }
  master.servers = NULL;

  if (master.debug & DEBUG_OTHER)
    printf ("lua cleanup\n");
  lua_close (internal_lua);
  internal_lua = NULL;
}

int lua_init () {
  int err;

  lua_State *L = luaL_newstate ();
  luaL_openlibs (L);

  int hin_lua_req_init (lua_State * L);
  hin_lua_req_init (L);
  int hin_lua_opt_init (lua_State * L);
  hin_lua_opt_init (L);
  int hin_lua_config_init (lua_State * L);
  hin_lua_config_init (L);

  if (run_file (L, HIN_CONF_PATH)) {
    printf ("internal error\n");
    return -1;
  }

  internal_lua = L;

  return 0;
}

int lua_reload () {
  hin_lua_clean ();
  lua_init ();
  return 0;
}


