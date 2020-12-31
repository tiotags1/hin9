
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "lua.h"
#include "ssl.h"

extern SSL_CTX * default_ctx;
SSL_CTX * hin_ssl_init (const char * cert, const char * key);

static int l_hin_create_httpd (lua_State *L) {
  int t = lua_type (L, 1);
  if (t != LUA_TFUNCTION) {
    printf ("not provided a callback\n");
    return 0;
  }
  lua_pushvalue (L, 1);
  int ref = luaL_ref (L, LUA_REGISTRYINDEX);

  hin_server_data_t * server = calloc (1, sizeof (hin_server_data_t));
  server->callback = ref;
  server->L = L;
  server->magic = HIN_SERVER_MAGIC;
  server->timeout = HTTPD_TIMEOUT;
  lua_pushlightuserdata (L, server);

  hin_server_data_t * prev = master.servers;
  server->next = prev;
  master.servers = server;

  return 1;
}

static int l_hin_listen (lua_State *L) {
  hin_server_data_t *server = (hin_server_data_t*)lua_touserdata (L, 1);
  if (server == NULL || server->magic != HIN_SERVER_MAGIC) {
    printf ("lua hin_listen need a valid server\n");
    //luaL_typerror(L, index, FOO);
    return 0;
  }
  const char * addr = lua_tostring (L, 2);
  const char * port = lua_tostring (L, 3);
  const char * type = lua_tostring (L, 4);
  const char * cert = lua_tostring (L, 5);
  const char * key = lua_tostring (L, 6);
  SSL_CTX * ctx = NULL;
  if (cert && key) {
    ctx = hin_ssl_init (cert, key);
    if (ctx == NULL) return 0;
  } else {
    int ssl = lua_toboolean (L, 5);
    if (ssl) {
      if (default_ctx == NULL) {
        printf ("ssl not init\n");
        return 0;
      }
    }
  }

  hin_client_t * sock = httpd_create (addr, port, type, ctx);
  sock->parent = server;
  lua_pushlightuserdata (L, sock);

  return 1;
}

static int l_hin_init_ssl (lua_State *L) {
  const char * cert = lua_tostring (L, 1);
  const char * key = lua_tostring (L, 2);
  SSL_CTX * ctx = NULL;
  if (cert && key) {
    ctx = hin_ssl_init (cert, key);
    if (ctx == NULL) return 0;
  } else {
    printf ("need a cert and a key file\n");
    return 0;
  }
  default_ctx = ctx;

  return 0;
}

static int l_log (lua_State *L) {
  FILE * fp = lua_touserdata (L, lua_upvalueindex (1));
  printf ("fp is %p\n", fp);
  return 0;
}

static int l_hin_create_log (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  FILE * fp = fopen (path, "w");
  if (fp == NULL) { printf ("can't open '%s'\n", path); return 0; }

  printf ("fp create %p\n", fp);
  lua_pushlightuserdata (L, fp);
  lua_pushcclosure (L, l_log, 1);

  return 1;
}

static lua_function_t functs [] = {
{"create_httpd",	l_hin_create_httpd },
{"listen",		l_hin_listen },
{"init_ssl",		l_hin_init_ssl },
{"create_log",		l_hin_create_log },
{NULL, NULL},
};

int hin_lua_config_init (lua_State * L) {
  lua_add_functions (L, functs);
}

