
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "vhost.h"
#include "system/hin_lua.h"

static int httpd_vhost_map_add_to_end_list (httpd_vhost_map_t ** list, httpd_vhost_map_t * new) {
  new->next = new->prev = NULL;
  httpd_vhost_map_t * last = *list;
  while (last && last->next) {
    last = last->next;
  }
  if (last == NULL) {
    *list = new;
  } else {
    new->prev = last;
    (last)->next = new;
  }
  return 0;
}

static int httpd_vhost_map_free (httpd_vhost_map_t * map) {
  if (map->pattern) free ((void*)map->pattern);
  free (map);
  return 0;
}

void httpd_vhost_map_clean (httpd_vhost_t * vhost) {
  httpd_vhost_map_t * next;
  for (httpd_vhost_map_t * map = vhost->map_start; map; map = next) {
    next = map->next;
    httpd_vhost_map_free (map);
  }
  for (httpd_vhost_map_t * map = vhost->map_finish; map; map = next) {
    next = map->next;
    httpd_vhost_map_free (map);
  }
  vhost->map_start = NULL;
  vhost->map_finish = NULL;
}

int httpd_vhost_map_callback (httpd_client_t * http, int type) {
  httpd_vhost_t * vhost = http->vhost;
  lua_State * L = vhost->L;

  string_t source = http->headers;
  string_t path;
  if (match_string (&source, "%a+ ("HIN_HTTP_PATH_ACCEPT")", &path) <= 0) {}
  const char * max = path.ptr + path.len;

  while (vhost) {
    httpd_vhost_map_t * map_start;
    switch (type) {
    case HIN_VHOST_MAP_START: map_start = vhost->map_start; break;
    case HIN_VHOST_MAP_FINISH: map_start = vhost->map_finish; break;
    default: printf ("error! %d", 32534543); map_start = NULL; break;
    }
    for (httpd_vhost_map_t * map = map_start; map; map = map->next) {
      const char * pattern = map->pattern;
      int skip = 1;
      for (const char * ptr = path.ptr; 1; ptr++) {
        if (*pattern == '*') { skip = 0; break; }
        if (ptr >= max && *pattern == '\0') { skip = 0; break; }
        if (ptr >= max) { skip = 1; break; }
        if (*pattern == '\0') { skip = 1; break; }
        if (*ptr != *pattern) { skip = 1; break; }
        pattern++;
      }
      if (skip) continue;

      lua_rawgeti (L, LUA_REGISTRYINDEX, map->callback);
      lua_pushlightuserdata (L, http);

      if (lua_pcall (L, 1, 1, 0) != 0) {
        printf ("error! map callback '%s' '%s'\n", vhost->hostname, lua_tostring (L, -1));
        lua_pop (L, 1);
        return -1;
      }
      int ret = 1;
      if (lua_isboolean (L, -1) && lua_toboolean (L, -1)) {
        ret = 0;
      }
      lua_pop (L, 1);
      if (ret <= 0) return ret;
    }
    vhost = vhost->parent;
  }
  return 1;
}

int l_hin_map (lua_State *L) {
  httpd_vhost_t * vhost = (httpd_vhost_t*)lua_touserdata (L, 1);
  if (vhost == NULL || vhost->magic != HIN_VHOST_MAGIC) {
    return luaL_error (L, "invalid vhost");
  }
  const char * path = lua_tostring (L, 2);
  if (master.debug & DEBUG_CONFIG)
    printf ("add map for %s:%s\n", vhost->hostname, path);

  if (!lua_isfunction (L, 4)) {
    return luaL_error (L, "requires a callback");
  }
  lua_pushvalue (L, 4);
  int callback = luaL_ref (L, LUA_REGISTRYINDEX);

  httpd_vhost_map_t * map = calloc (1, sizeof (*map));
  map->pattern = strdup (path);
  map->state = lua_tonumber (L, 3);
  map->vhost = vhost;
  map->lua = L;
  map->callback = callback;

  switch (map->state) {
  case 0:
    httpd_vhost_map_add_to_end_list (&vhost->map_start, map);
  break;
  case 99:
    httpd_vhost_map_add_to_end_list (&vhost->map_finish, map);
  break;
  default:
    httpd_vhost_map_free (map);
    luaL_error (L, "requires a valid state");
  }

  return 0;
}

