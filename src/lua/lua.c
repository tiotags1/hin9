
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "lua.h"

int hin_server_callback (hin_client_t * client) {
  if (client->parent == NULL) {
    printf ("no socket parent\n");
    return -1;
  }
  hin_client_t * server_client = (hin_client_t*)client->parent;
  hin_server_data_t * server = (hin_server_data_t*)server_client->parent;

  lua_State * L = server->L;
  lua_rawgeti (L, LUA_REGISTRYINDEX, server->callback);
  lua_pushlightuserdata (L, server);
  lua_pushlightuserdata (L, client);

  if (lua_pcall (L, 2, LUA_MULTRET, 0) != 0) {
    printf ("error running callback '%s'\n", lua_tostring (L, -1));
    return -1;
  }
}

void lua_server_clean (hin_server_data_t * server) {
  lua_State * L = server->L;
  luaL_unref (L, LUA_REGISTRYINDEX, server->callback);

  // TODO this is bad
  lua_close (L);
  printf ("closed lua\n");

  free (server);
}

void hin_lua_clean () {
  hin_server_data_t * elem = master.servers;
  hin_server_data_t * next;
  for (;elem; elem = next) {
    next = elem->next;
    lua_server_clean (elem);
  }
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

  if (run_file (L, "workdir/main.lua")) {
    printf ("internal error\n");
    return -1;
  }

  return 0;
}


