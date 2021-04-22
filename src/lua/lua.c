
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>

#include "hin.h"
#include "hin_lua.h"
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
  return 0;
}

#include "http.h"

int hin_server_error_callback (hin_client_t * client, int error_code, const char * msg) {
  if (client->parent == NULL) {
    printf ("no socket parent\n");
    return -1;
  }
  hin_client_t * server_client = (hin_client_t*)client->parent;
  hin_server_data_t * server = (hin_server_data_t*)server_client->parent;

  lua_State * L = server->L;
  if (server->error_callback == 0) return 0;

  httpd_client_t * http = (httpd_client_t*)client;
  if (http->state & HIN_REQ_ERROR_HANDLED) return 0;
  http->state |= HIN_REQ_ERROR_HANDLED;

  http->state &= ~(HIN_REQ_DATA | HIN_REQ_ERROR);

  lua_rawgeti (L, LUA_REGISTRYINDEX, server->error_callback);
  lua_pushlightuserdata (L, server);
  lua_pushlightuserdata (L, client);
  lua_pushnumber (L, error_code);
  lua_pushstring (L, msg);

  if (lua_pcall (L, 4, LUA_MULTRET, 0) != 0) {
    printf ("error running error callback '%s'\n", lua_tostring (L, -1));
    return -1;
  }
  if (http->state & HIN_REQ_DATA) return 1;
  return 0;
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
    printf ("error running finish callback '%s'\n", lua_tostring (L, -1));
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
  if (server->error_callback)
    luaL_unref (L, LUA_REGISTRYINDEX, server->error_callback);

  if (server->hostname) free (server->hostname);
  if (server->cwd_path) free (server->cwd_path);
  if (server->cwd_fd && server->cwd_fd != AT_FDCWD) close (server->cwd_fd);
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

  lua_close (internal_lua);
  internal_lua = NULL;
}

int lua_init () {
  int err = 0;

  lua_State *L = luaL_newstate ();
  if (L == NULL) return -1;
  luaL_openlibs (L);

  int hin_lua_req_init (lua_State * L);
  err |= hin_lua_req_init (L);
  int hin_lua_opt_init (lua_State * L);
  err |= hin_lua_opt_init (L);
  int hin_lua_config_init (lua_State * L);
  err |= hin_lua_config_init (L);

  if (err < 0) return err;

  lua_pushstring (L, master.logdir_path);
  lua_setglobal (L, "logdir");
  lua_pushstring (L, master.cwd_path);
  lua_setglobal (L, "cwd");

  internal_lua = L;

  return 0;
}

int hin_conf_load (const char * path) {
  if (run_file (internal_lua, path)) {
    printf ("can't load config at '%s'\n", path);
    return -1;
  }
  return 0;
}

int lua_reload () {
  hin_lua_clean ();
  lua_init ();
  return 0;
}


