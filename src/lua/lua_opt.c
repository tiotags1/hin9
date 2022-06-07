
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hin.h"
#include "http.h"
#include "file.h"
#include "vhost.h"
#include "system/hin_lua.h"

#include <basic_hashtable.h>

int hin_lua_mask_from_str (lua_State * L, int pos, uint32_t * ptr1) {
  if (lua_isnumber (L, pos)) {
    *ptr1 = lua_tonumber (L, pos);
    return 1;
  }
  const char * mask = lua_tostring (L, pos);
  if (mask) {
    char * ptr = NULL;
    uint32_t nr = strtol (mask, &ptr, 16);
    if (ptr <= mask) {
      return luaL_error (L, "can't parse mask");
    }
    *ptr1 = nr;
    return 1;
  } else {
    return luaL_error (L, "mask needs to be a string or number");
  }
  return -1;
}

static uint32_t get_mask (lua_State * L, const char * name) {
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
  } else if (strcmp (name, "gzip") == 0) {
    return HIN_HTTP_GZIP;
  } else if (strcmp (name, "deflate") == 0) {
    return HIN_HTTP_DEFLATE;
  } else if (strcmp (name, "compress") == 0) {
    return HIN_HTTP_COMPRESS;
  } else if (strcmp (name, "date") == 0) {
    return HIN_HTTP_DATE;
  } else if (strcmp (name, "banner") == 0) {
    return HIN_HTTP_BANNER;
  } else if (strcmp (name, "chunked_upload") == 0) {
    return HIN_HTTP_CHUNKED_UPLOAD;
  } else if (strcmp (name, "local_cache") == 0) {
    return HIN_HTTP_LOCAL_CACHE;
  } else if (strcmp (name, "all") == 0) {
    return ~0;
  } else {
    return luaL_error (L, "value unknown");
  }
}

static int l_hin_get_server_option (lua_State *L) {
  httpd_vhost_t *client = (httpd_vhost_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_VHOST_MAGIC) {
    return luaL_error (L, "requires valid vhost");
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) {
    return luaL_error (L, "missing option name");
  }

  if (strcmp (name, "enable") == 0) {
    const char * param = lua_tostring (L, 3);
    int ret = client->disable & get_mask (L, param);
    lua_pushboolean (L, ret);
    return 1;
  } else {
    return luaL_error (L, "unknown option '%s'", name);
  }
  return 0;
}

static int l_hin_set_server_option (lua_State *L) {
  httpd_vhost_t *client = (httpd_vhost_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_VHOST_MAGIC) {
    return luaL_error (L, "requires valid vhost");
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) {
    return luaL_error (L, "missing option name");
  }

  if (strcmp (name, "enable") == 0) {
    const char * param = lua_tostring (L, 3);
    client->disable &= ~get_mask (L, param);
    if (client->debug & DEBUG_CONFIG) printf ("lua server enable %s\n", param);
    return 0;
  } else if (strcmp (name, "disable") == 0) {
    const char * param = lua_tostring (L, 3);
    client->disable |= get_mask (L, param);
    if (client->debug & DEBUG_CONFIG) printf ("lua server disable %s\n", param);
    return 0;
  } else if (strcmp (name, "timeout") == 0) {
    int timeout = lua_tonumber (L, 3);
    client->timeout = timeout;
    if (client->debug & DEBUG_CONFIG) printf ("lua server timeout set to %d\n", timeout);
    return 0;
  } else if (strcmp (name, "hostname") == 0) {
    const char * name = lua_tostring (L, 3);
    if (client->hostname) free (client->hostname);
    client->hostname = strdup (name);
    if (client->debug & DEBUG_CONFIG) printf ("lua server hostname set to %s\n", name);
    return 0;
  } else if (strcmp (name, "debug") == 0) {
    if (hin_lua_mask_from_str (L, 3, &client->debug) >= 0) {
    }
    return 0;
  } else if (strcmp (name, "cwd") == 0) {
    httpd_vhost_set_work_dir (client, lua_tostring (L, 3));
    return 0;

  } else if (strcmp (name, "directory_listing") == 0) {
    int ret = lua_toboolean (L, 3);
    if (client->debug & DEBUG_CONFIG) printf ("lua server directory listing %s\n", ret ? "on" : "off");
    client->vhost_flags = (client->vhost_flags & (~HIN_DIRECTORY_LISTING)) | (HIN_DIRECTORY_LISTING * ret);
    return 0;

  } else if (strcmp (name, "directory_no_redirect") == 0) {
    int ret = lua_toboolean (L, 3);
    if (client->debug & DEBUG_CONFIG) printf ("lua server directory no redirect %s\n", ret ? "on" : "off");
    client->vhost_flags = (client->vhost_flags & (~HIN_DIRECTORY_NO_REDIRECT)) | (HIN_DIRECTORY_NO_REDIRECT * ret);
    return 0;

  } else {
    return luaL_error (L, "unkown option '%s'", name);
  }
  return 0;
}

static int l_hin_get_option (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) {
    return luaL_error (L, "missing option name");
  }

  httpd_client_t * http = (httpd_client_t*)client;
  if (strcmp (name, "id") == 0) {
    lua_pushnumber (L, http->c.sockfd);
    return 1;
  } else if (strcmp (name, "status") == 0) {
    lua_pushnumber (L, http->status);
    return 1;
  } else if (strcmp (name, "keepalive") == 0) {
    if (http->peer_flags & HIN_HTTP_KEEPALIVE) lua_pushboolean (L, 1);
    else lua_pushboolean (L, 0);
    return 1;
  } else if (strcmp (name, "enable") == 0) {
    const char * param = lua_tostring (L, 3);
    int ret = http->disable & get_mask (L, param);
    lua_pushboolean (L, ret);
    return 1;

  } else if (strcmp (name, "response_size") == 0) {
    lua_pushnumber (L, http->count);
    return 1;

  } else if (strcmp (name, "vhost") == 0) {
    httpd_vhost_t * vhost = http->vhost;
    lua_pushstring (L, vhost->hostname);
    return 1;

  } else {
    return luaL_error (L, "unkown option '%s'", name);
  }
  return 0;
}

static int l_hin_set_option (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  const char * name = lua_tostring (L, 2);
  if (name == NULL) {
    return luaL_error (L, "missing option name");
  }

  httpd_client_t * http = (httpd_client_t*)client;
  if (strcmp (name, "enable") == 0) {
    http->disable &= ~get_mask (L, lua_tostring (L, 3));
    return 0;
  } else if (strcmp (name, "disable") == 0) {
    http->disable |= get_mask (L, lua_tostring (L, 3));
    return 0;
  } else if (strcmp (name, "status") == 0) {
    http->status = lua_tonumber (L, 3);
    return 0;
  } else if (strcmp (name, "cache_key") == 0) {
    if (lua_isnil (L, 3)) {
      http->cache_key1 = http->cache_key2 = 0;
      return 0;
    }
    size_t len = 0;
    const char * str = lua_tolstring (L, 3, &len);
    uintptr_t hin_cache_seed ();
    uintptr_t seed = hin_cache_seed ();
    basic_ht_hash (str, len, seed, &http->cache_key1, &http->cache_key2);
    for (int i=4; i <= lua_gettop (L); i++) {
      str = lua_tolstring (L, i, &len);
      basic_ht_hash_continue (str, len, &http->cache_key1, &http->cache_key2);
    }
    return 0;
  } else if (strcmp (name, "cache") == 0) {
    if (lua_isnumber (L, 3)) {
      void hin_cache_set_number (httpd_client_t * http, time_t num);
      hin_cache_set_number (http, lua_tonumber (L, 3));
    } else {
      size_t len = 0;
      const char * str = lua_tolstring (L, 3, &len);
      httpd_parse_cache_str (str, len, &http->cache_flags, &http->cache);
    }
    return 0;
  } else if (strcmp (name, "keepalive") == 0) {
    int value = lua_toboolean (L, 3);
    if (value) {  }
    else { http->peer_flags &= ~HIN_HTTP_KEEPALIVE; }
    return 0;

  } else if (strcmp (name, "debug") == 0) {
    if (hin_lua_mask_from_str (L, 3, &http->debug) >= 0) {
    }
    return 0;

  } else {
    return luaL_error (L, "unknown option '%s'", name);
  }
  return 0;
}

static int l_hin_set_global_option (lua_State *L) {
  const char * name = lua_tostring (L, 1);
  if (name == NULL) {
    return luaL_error (L, "missing option name");
  }

  if (strcmp (name, "create_directory") == 0) {
    int val = lua_toboolean (L, 2);
    master.flags = (master.flags & (~HIN_CREATE_DIRECTORY)) | (HIN_CREATE_DIRECTORY * val);
    if (master.debug & DEBUG_CONFIG) printf ("lua global %s %s\n", name, val ? "on" : "off");
    return 0;

  } else if (strcmp (name, "verbose_errors") == 0) {
    int val = lua_toboolean (L, 2);
    master.flags = (master.flags & (~HIN_VERBOSE_ERRORS)) | (HIN_VERBOSE_ERRORS * val);
    if (master.debug & DEBUG_CONFIG) printf ("lua global %s %s\n", name, val ? "on" : "off");
    return 0;

  } else {
    return luaL_error (L, "unknown option '%s'", name);
  }
  return 0;
}

static uint32_t get_debug_mask (lua_State *L, const char * str) {
  uint32_t mask = 0;
  if (strcmp (str, "basic") == 0) {
    mask |= DEBUG_BASIC;
  } else if (strcmp (str, "config") == 0) {
    mask |= DEBUG_CONFIG;
  } else if (strcmp (str, "vfs") == 0) {
    mask |= DEBUG_VFS;
  } else if (strcmp (str, "socket") == 0) {
    mask |= DEBUG_SOCKET;
  } else if (strcmp (str, "uring") == 0) {
    mask |= DEBUG_URING;
  } else if (strcmp (str, "ssl") == 0) {
    mask |= DEBUG_SSL;
  } else if (strcmp (str, "syscall") == 0) {
    mask |= DEBUG_SYSCALL;
  } else if (strcmp (str, "memory") == 0) {
    mask |= DEBUG_MEMORY;
  } else if (strcmp (str, "http") == 0) {
    mask |= DEBUG_HTTP;
  } else if (strcmp (str, "cgi") == 0) {
    mask |= DEBUG_CGI;
  } else if (strcmp (str, "proxy") == 0) {
    mask |= DEBUG_PROXY;
  } else if (strcmp (str, "http_filter") == 0) {
    mask |= DEBUG_HTTP_FILTER;
  } else if (strcmp (str, "post") == 0) {
    mask |= DEBUG_POST;
  } else if (strcmp (str, "child") == 0) {
    mask |= DEBUG_CHILD;
  } else if (strcmp (str, "cache") == 0) {
    mask |= DEBUG_CACHE;
  } else if (strcmp (str, "timeout") == 0) {
    mask |= DEBUG_TIMEOUT;
  } else if (strcmp (str, "rw") == 0) {
    mask |= DEBUG_RW;
  } else if (strcmp (str, "rw_error") == 0) {
    mask |= DEBUG_RW_ERROR;
  } else if (strcmp (str, "pipe") == 0) {
    mask |= DEBUG_PIPE;
  } else if (strcmp (str, "info") == 0) {
    mask |= DEBUG_INFO;
  } else {
    return luaL_error (L, "unknown mask '%s'", str);
  }
  return mask;
}

static int l_hin_enable_debug (lua_State *L) {
  uint32_t debug = 0, mask = 0;

  for (int i=2; i <= lua_gettop (L); i++) {
    if (!lua_isstring (L, i)) continue;
    const char * str = lua_tostring (L, i);
    mask |= get_debug_mask (L, str);
  }

  if (mask == 0) {
    if (hin_lua_mask_from_str (L, 1, &mask) < 0) {
      return 0;
    }
  }

  debug |= mask;

  lua_pushnumber (L, debug);

  return 1;
}

static int l_hin_disable_debug (lua_State *L) {
  uint32_t debug = 0, mask = 0;

  for (int i=2; i <= lua_gettop (L); i++) {
    if (!lua_isstring (L, i)) continue;
    const char * str = lua_tostring (L, i);
    mask |= get_debug_mask (L, str);
  }

  if (mask == 0) {
    if (hin_lua_mask_from_str (L, 1, &mask) < 0) {
      return 0;
    }
  }

  debug &= ~mask;

  lua_pushnumber (L, debug);

  return 1;
}

static lua_function_t functs [] = {
{"set_server_option",	l_hin_set_server_option },
{"get_server_option",	l_hin_get_server_option },
{"set_option",		l_hin_set_option },
{"get_option",		l_hin_get_option },
{"set_global_option",	l_hin_set_global_option },
{"enable_debug",	l_hin_enable_debug },
{"disable_debug",	l_hin_disable_debug },
{NULL, NULL},
};

int hin_lua_opt_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

