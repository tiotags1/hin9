
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "lua.h"
#include "http.h"

uint32_t get_mask (const char * name) {
  if (strcmp (name, "keepalive") == 0) {
    return HIN_HTTP_KEEPALIVE;
  } else if (strcmp (name, "range") == 0) {
    return HIN_HTTP_RANGE;
  } else if (strcmp (name, "modified_since") == 0) {
    return HIN_HTTP_MODIFIED;
  } else if (strcmp (name, "etag") == 0) {
    return HIN_HTTP_ETAG;
  } else if (strcmp (name, "cache") == 0) {
    return HIN_HTTP_CACHE;
  } else if (strcmp (name, "post") == 0) {
    return HIN_HTTP_POST;
  } else if (strcmp (name, "chunked") == 0) {
    return HIN_HTTP_CHUNKED;
  } else if (strcmp (name, "deflate") == 0) {
    return HIN_HTTP_DEFLATE;
  } else if (strcmp (name, "date") == 0) {
    return HIN_HTTP_DATE;
  } else if (strcmp (name, "all") == 0) {
    return 0xffffffff;
  } else {
    printf ("unknown option %s\n", name);
    return 0;
  }
}

static int l_hin_get_server_option (lua_State *L) {
  hin_server_data_t *client = (hin_server_data_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_SERVER_MAGIC) {
    printf ("lua hin_get_option need a valid server\n");
    return 0;
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) { printf ("option nil\n"); return 0; }

  if (1) {
    printf ("get_otion unknown option '%s'\n", name);
    return 0;
  }
  return 0;
}

static int l_hin_set_server_option (lua_State *L) {
  hin_server_data_t *client = (hin_server_data_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_SERVER_MAGIC) {
    printf ("lua hin_set_option need a valid server\n");
    return 0;
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) { printf ("option nil\n"); return 0; }

  if (strcmp (name, "enable") == 0) {
    const char * param = lua_tostring (L, 3);
    client->disable &= ~get_mask (param);
    if (master.debug & DEBUG_CONFIG) printf ("lua server enable %s\n", param);
    return 0;
  } else if (strcmp (name, "disable") == 0) {
    const char * param = lua_tostring (L, 3);
    client->disable |= get_mask (param);
    if (master.debug & DEBUG_CONFIG) printf ("lua server disable %s\n", param);
    return 0;
  } else if (strcmp (name, "timeout") == 0) {
    int timeout = lua_tonumber (L, 3);
    client->timeout = timeout;
    if (master.debug & DEBUG_CONFIG) printf ("lua server timeout set to %d\n", timeout);
    return 0;
  } else {
    printf ("set_otion unknown option '%s'\n", name);
    return 0;
  }
  return 0;
}

static int l_hin_get_option (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_get_option need a valid client\n");
    return 0;
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) { printf ("option nil\n"); return 0; }

  httpd_client_t * http = (httpd_client_t*)client->extra;
  if (strcmp (name, "keepalive") == 0) {
    if (http->disable & HIN_HTTP_KEEPALIVE) lua_pushboolean (L, 1);
    else lua_pushboolean (L, 0);
    return 1;
  } else {
    printf ("get_otion unknown option '%s'\n", name);
    return 0;
  }
  return 0;
}

static int l_hin_set_option (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_set_option need a valid client\n");
    return 0;
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) { printf ("option nil\n"); return 0; }

  httpd_client_t * http = (httpd_client_t*)client->extra;
  if (strcmp (name, "keepalive") == 0) {
    int value = lua_toboolean (L, 3);
    printf ("set keepalive to %d\n", value);
    if (value) {  }
    else { http->peer_flags &= ~HIN_HTTP_KEEPALIVE; }
    return 0;
  } else if (strcmp (name, "status") == 0) {
    http->status = lua_tonumber (L, 3);
    return 0;
  } else if (strcmp (name, "cache") == 0) {
    http->cache = lua_tonumber (L, 3);
    return 0;
  } else if (strcmp (name, "enable") == 0) {
    http->disable &= ~get_mask (lua_tostring (L, 3));
    return 0;
  } else if (strcmp (name, "disable") == 0) {
    http->disable |= get_mask (lua_tostring (L, 3));
    return 0;
  } else {
    printf ("set_otion unknown option '%s'\n", name);
    return 0;
  }
  return 0;
}


static lua_function_t functs [] = {
{"set_server_option",	l_hin_set_server_option },
{"get_server_option",	l_hin_get_server_option },
{"set_option",		l_hin_set_option },
{"get_option",		l_hin_get_option },
{NULL, NULL},
};

int hin_lua_opt_init (lua_State * L) {
  lua_add_functions (L, functs);
}

