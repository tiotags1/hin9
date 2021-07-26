
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>

#include "hin.h"
#include "conf.h"
#include "http.h"
#include "vhost.h"
#include "system/hin_lua.h"

int hin_server_callback (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)client;
  hin_vhost_t * vhost = http->vhost;
  for (; vhost; vhost = vhost->parent) {
    if (vhost->request_callback) { break; }
    if (vhost->parent == NULL) break;
  }

  hin_server_t * socket = client->parent;
  if (vhost->hsts &&
     (vhost->vhost_flags & HIN_HSTS_NO_REDIRECT) == 0 &&
     socket->ssl_ctx == NULL) {
    httpd_respond_redirect_https (http);
    return 0;
  }

  lua_State * L = vhost->L;
  lua_rawgeti (L, LUA_REGISTRYINDEX, vhost->request_callback);
  lua_pushlightuserdata (L, client);

  if (lua_pcall (L, 1, LUA_MULTRET, 0) != 0) {
    printf ("error! request callback '%s' '%s'\n", vhost->hostname, lua_tostring (L, -1));
    return -1;
  }
  return 0;
}

int hin_server_error_callback (hin_client_t * client, int error_code, const char * msg) {
  httpd_client_t * http = (httpd_client_t*)client;
  hin_vhost_t * vhost = http->vhost;
  for (; vhost; vhost = vhost->parent) {
    if (vhost->error_callback) { break; }
    if (vhost->parent == NULL) break;
  }
  if (vhost->error_callback == 0) return 0;

  lua_State * L = vhost->L;

  if (http->state & HIN_REQ_ERROR_HANDLED) return 0;
  http->state |= HIN_REQ_ERROR_HANDLED;
  http->state &= ~(HIN_REQ_DATA | HIN_REQ_ERROR);

  lua_rawgeti (L, LUA_REGISTRYINDEX, vhost->error_callback);
  lua_pushlightuserdata (L, client);
  lua_pushnumber (L, error_code);
  lua_pushstring (L, msg);

  if (lua_pcall (L, 3, LUA_MULTRET, 0) != 0) {
    printf ("error! error callback '%s' '%s'\n", vhost->hostname, lua_tostring (L, -1));
    return -1;
  }
  if (http->state & HIN_REQ_DATA) return 1;
  return 0;
}

int hin_server_finish_callback (hin_client_t * client) {
  httpd_client_t * http = (httpd_client_t*)client;
  hin_vhost_t * vhost = http->vhost;
  for (; vhost; vhost = vhost->parent) {
    if (vhost->error_callback) { break; }
    if (vhost->parent == NULL) break;
  }
  if (vhost->finish_callback == 0) return 0;

  lua_State * L = vhost->L;
  if (vhost->finish_callback == 0) return 0;
  lua_rawgeti (L, LUA_REGISTRYINDEX, vhost->finish_callback);
  lua_pushlightuserdata (L, client);

  if (lua_pcall (L, 1, LUA_MULTRET, 0) != 0) {
    printf ("error! finish callback '%s' '%s'\n", vhost->hostname, lua_tostring (L, -1));
    return -1;
  }
  return 0;
}

static lua_State * internal_lua = NULL;

void hin_lua_report_error () {
  lua_State *L = internal_lua;
  const char * reason = lua_tostring (L, -1);
  printf ("error! '%s'\n", reason);
}

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

void hin_ssl_ctx_unref (hin_ssl_ctx_t * box) {
  box->refcount--;
  if (box->refcount > 0) { return ; }
  free ((void*)box->cert);
  free ((void*)box->key);
  // free ssl ctx
  free (box);
}

void lua_server_clean (hin_vhost_t * server) {
  lua_State * L = server->L;
  if (server->request_callback)
    luaL_unref (L, LUA_REGISTRYINDEX, server->request_callback);
  if (server->finish_callback)
    luaL_unref (L, LUA_REGISTRYINDEX, server->finish_callback);
  if (server->error_callback)
    luaL_unref (L, LUA_REGISTRYINDEX, server->error_callback);

  hin_ssl_ctx_t * box = server->ssl;
  if (box)
    hin_ssl_ctx_unref (box);

  if (server->hostname) free (server->hostname);
  if (server->cwd_fd && server->cwd_fd != AT_FDCWD) close (server->cwd_fd);
  free (server);
}

void hin_lua_clean () {
  hin_vhost_t * elem = master.vhosts;
  hin_vhost_t * next;
  for (;elem; elem = next) {
    next = elem->next;
    lua_server_clean (elem);
  }
  master.vhosts = NULL;

  hin_ssl_ctx_t * box = master.certs;
  while (box) {
    hin_ssl_ctx_t * next = box->next;
    hin_ssl_ctx_unref (box);
    box = next;
  }

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
  int hin_lua_os_init (lua_State * L);
  err |= hin_lua_os_init (L);
  int hin_lua_utils_init (lua_State * L);
  err |= hin_lua_utils_init (L);

  if (err < 0) return err;

  lua_pushstring (L, master.logdir_path);
  lua_setglobal (L, "logdir");
  lua_pushstring (L, master.workdir_path);
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

int hin_lua_run (const char * data, int len) {
  lua_State * L = internal_lua;
  int ret = hin_lua_run_string (L, data, len, "console");
  if (ret < 0) {
    fprintf (stderr, "error! lua parsing '%.*s': %s\n", len, data, lua_tostring (L, -1));
  }
  return ret;
}

int lua_reload () {
  hin_lua_clean ();
  lua_init ();
  return 0;
}


