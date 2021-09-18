
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "conf.h"
#include "vhost.h"
#include "system/hin_lua.h"

#include <fcntl.h>

static hin_vhost_t * default_parent = NULL;

static const char * l_hin_get_str (lua_State *L, int tpos, const char * name) {
  const char * ret = NULL;
  lua_pushstring (L, name);
  lua_gettable (L, tpos);
  if (lua_type (L, -1) == LUA_TSTRING) {
    ret = lua_tostring (L, -1);
  }
  lua_pop (L, 1);
  return ret;
}

static int l_hin_add_socket (lua_State *L, hin_vhost_t * vhost, int tpos) {
  if (lua_type (L, tpos) != LUA_TTABLE) {
    return -1;
  }
  int ssl = 0;
  const char * bind = l_hin_get_str (L, tpos, "bind");
  const char * port = l_hin_get_str (L, tpos, "port");
  const char * sock_type = l_hin_get_str (L, tpos, "sock_type");

  lua_pushstring (L, "ssl");
  lua_gettable (L, tpos);
  if (lua_type (L, -1) == LUA_TBOOLEAN) {
    ssl = lua_toboolean (L, -1);
  }
  lua_pop (L, 1);

  void * ctx = NULL;
  if (ssl) {
    hin_ssl_ctx_t * box = vhost->ssl;
    if (box)
      ctx = box->ctx;
    if (ctx == NULL) {
      #if HIN_HTTPD_ERROR_ON_MISSING_CERT
      return luaL_error (L, "%s:%s lacks cert\n", bind, port);
      #else
      printf ("error! vhost %s:%s lacks cert\n", bind, port);
      #endif
      return 0;
    }
  }

  hin_server_t * sock = httpd_create (bind, port, sock_type, ctx);
  sock->c.parent = vhost;

  return 0;
}

static int l_hin_vhost_get_callback (lua_State *L, int tpos, const char * name) {
  int ret = 0;
  lua_pushstring (L, name);
  lua_gettable (L, tpos);
  if (lua_type (L, -1) == LUA_TNIL) {
    char buffer[60];
    snprintf (buffer, sizeof buffer, "default_%s_handler", name);
    lua_getglobal (L, buffer);
  }
  if (lua_type (L, -1) != LUA_TFUNCTION) {
    lua_pop (L, 1);
  } else {
    ret = luaL_ref (L, LUA_REGISTRYINDEX);
  }
  return ret;
}

static void hin_vhost_free (hin_vhost_t * vhost) {
  if (vhost == NULL) return;
  free (vhost);
}

int l_hin_add_vhost (lua_State *L) {
  if (lua_type (L, 1) != LUA_TTABLE) {
    return luaL_error (L, "requires a table");
  }

  // TODO check if this is an actual ssl context
  hin_vhost_t * parent = NULL;
  size_t len = 0;
  int nsocket = 0;
  const char * htdocs = NULL;

  lua_pushstring (L, "parent");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TLIGHTUSERDATA) {
    parent = (void*)lua_topointer (L, -1);
  }
  lua_pop (L, 1);

  hin_vhost_t * vhost = calloc (1, sizeof (hin_vhost_t));

  lua_pushstring (L, "cert");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TLIGHTUSERDATA) {
    hin_ssl_ctx_t * box = (void*)lua_topointer (L, -1);
    if (box && box->magic == HIN_CERT_MAGIC) {
      vhost->ssl = box;
      vhost->ssl_ctx = box->ctx;
      box->refcount++;
    }
  } else if (lua_type (L, -1) == LUA_TBOOLEAN) {
    #if HIN_HTTPD_ERROR_ON_MISSING_CERT
    return luaL_error (L, "invalid cert\n");
    #else
    printf ("error! vhost invalid cert\n");
    #endif
    vhost->ssl = vhost->ssl_ctx = NULL;
  }
  lua_pop (L, 1);

  // listen to sockets
  lua_pushstring (L, "socket");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TTABLE) {
    int sz = hin_lua_rawlen (L, -1);
    for (int i = 1; i <= sz; i++) {
      lua_rawgeti (L, -1, i);
      if (l_hin_add_socket (L, vhost, lua_gettop (L)) >= 0) {
        nsocket++;
      } else {
        return luaL_error (L, "invalid socket");
      }
      lua_pop (L, 1);
    }
    if (default_parent == NULL) {
      default_parent = vhost;
    }
  }
  lua_pop (L, 1);

  if (parent == NULL && default_parent && default_parent != vhost) {
    parent = default_parent;
  }

  if (nsocket == 0) {
    if (parent == NULL) {
      hin_vhost_free (vhost);
      return luaL_error (L, "requires parent");
    } else if (parent->magic != HIN_VHOST_MAGIC) {
      hin_vhost_free (vhost);
      return luaL_error (L, "requires valid parent %x!=%x", parent->magic, HIN_VHOST_MAGIC);
    }
  }

  lua_pushstring (L, "host");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TTABLE) {
    int sz = hin_lua_rawlen (L, -1);
    for (int i = 1; i <= sz; i++) {
      lua_rawgeti (L, -1, i);
      const char * name = lua_tolstring (L, -1, &len);
      if (i == 1) {
        vhost->hostname = strdup (name);
      }
      if (master.debug & DEBUG_HTTP)
        printf ("vhost '%s'\n", name);
      if (hin_vhost_get (name, len)) {
        return luaL_error (L, "vhost duplicate '%s'", name);
      }
      int ret = hin_vhost_add (name, len, vhost);
      if (ret < 0) {
        // TODO cancel the whole host
        return luaL_error (L, "vhost add '%s'\n", name);
      }
      lua_pop (L, 1);
    }
  }
  lua_pop (L, 1);

  lua_pushstring (L, "htdocs");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TSTRING) {
    htdocs = lua_tostring (L, -1);
  } else {
    htdocs = master.workdir_path;
  }
  lua_pop (L, 1);

  lua_pushstring (L, "hsts");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TNUMBER) {
    vhost->hsts = lua_tonumber (L, -1);
  }
  lua_pop (L, 1);

  lua_pushstring (L, "hsts_flags");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TSTRING) {
    string_t line, param;
    line.len = 0;
    line.ptr = (char*)lua_tolstring (L, -1, &line.len);
    while (match_string (&line, "([%w_]+)%s*", &param) > 0) {
      if (match_string (&param, "subdomains") > 0) {
        vhost->vhost_flags |= HIN_HSTS_SUBDOMAINS;
      } else if (match_string (&param, "preload") > 0) {
        vhost->vhost_flags |= HIN_HSTS_PRELOAD;
      } else if (match_string (&param, "no_redirect") > 0) {
        vhost->vhost_flags |= HIN_HSTS_NO_REDIRECT;
      } else if (match_string (&param, "no_header") > 0) {
        vhost->vhost_flags |= HIN_HSTS_NO_HEADER;
      }
    }
  }
  lua_pop (L, 1);

  vhost->request_callback = l_hin_vhost_get_callback (L, 1, "onRequest");
  vhost->error_callback = l_hin_vhost_get_callback (L, 1, "onError");
  vhost->finish_callback = l_hin_vhost_get_callback (L, 1, "onFinish");

  vhost->L = L;
  vhost->magic = HIN_VHOST_MAGIC;
  vhost->timeout = HIN_HTTPD_TIMEOUT;
  if (parent) {
    vhost->debug = parent->debug;
    vhost->disable = parent->disable;
  } else {
    vhost->debug = master.debug;
  }
  vhost->parent = parent;
  lua_pushlightuserdata (L, vhost);

  hin_server_set_work_dir (vhost, htdocs);

  hin_vhost_t * prev = master.vhosts;
  vhost->next = prev;
  master.vhosts = vhost;

  return 1;
}


