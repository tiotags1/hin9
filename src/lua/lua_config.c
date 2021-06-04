
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "hin_lua.h"
#include "ssl.h"
#include "conf.h"

#include <fcntl.h>

hin_buffer_t * logs = NULL;

static int hin_log_write_callback (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    printf ("log write error '%s'\n", strerror (-ret));
    return -1;
  }
  if (ret < buf->count) {
    buf->ptr += ret;
    buf->count -= ret;
    if (buf->flags & HIN_OFFSETS)
      buf->pos += ret;
    if (hin_request_write (buf) < 0) {
      buf->flags |= HIN_SYNC;
      hin_request_write (buf);
    }
    return 0;
  }
  return 1;
}

static int hin_log_flush_single (hin_buffer_t * buf) {
  lseek (buf->fd, buf->pos, SEEK_SET);
  int ret = write (buf->fd, buf->buffer, buf->count);
  if (ret < 0) { perror ("log write"); }
  return 0;
}

static int l_log (lua_State *L) {
  hin_buffer_t * buf = lua_touserdata (L, lua_upvalueindex (1));
  size_t len = 0;
  const char * fmt = lua_tolstring (L, 1, &len);
  const char * max = fmt + len;
  if (fmt == NULL) { printf ("fmt nil\n"); return 0; }

  time_t t;
  time (&t);

  char buffer1[80];
  struct tm data;
  struct tm *info = gmtime_r (&t, &data);
  if (info == NULL) { perror ("gmtime_r"); return 0; }
  int ret1 = strftime (buffer1, sizeof buffer1, "%F %R ", info);
  header_raw (buf, buffer1, ret1);

  const char * last = fmt;
  char buffer[30];
  int ret = 2;
  for (const char * ptr = fmt; ptr < max; ptr++) {
    if (*ptr == '%') {
      int prev_len = ptr - last;
      header_raw (buf, last, prev_len);
      ptr++;
      switch (*ptr) {
      case 's':
        buffer[0] = '%';
        buffer[1] = 's';
        buffer[2] = '\0';
        header (buf, buffer, lua_tostring (L, ret++));
      break;
      case 'd':
      case 'x':
      case 'p':
      case 'D':
      case 'X':
        buffer[0] = '%';
        buffer[1] = *ptr;
        buffer[2] = '\0';
        header (buf, buffer, lua_tointeger (L, ret++));
      break;
      default: header (buf, "%%%c", *ptr);
      }
      last = ptr+1;
    }
  }
  header_raw (buf, last, max-last);

  if (buf->next == NULL) { return 0; }

  lua_pushlightuserdata (L, buf->next);
  lua_replace (L, lua_upvalueindex (1));

  logs = buf->next;
  buf->next->pos = buf->pos + buf->count;

  if (hin_request_write (buf) < 0) {
    buf->flags |= HIN_SYNC;
    hin_request_write (buf);
  }

  return 0;
}

int hin_log_flush () {
  hin_log_flush_single (logs);
  close (logs->fd);
  hin_buffer_clean (logs);
  return 0;
}

static int l_hin_create_log (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  if (master.flags & HIN_PRETEND) path = "/dev/null";
  int fd = openat (AT_FDCWD, path, O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT, 0660);
  if (fd < 0) {
    printf ("can't open '%s' %s\n", path, strerror (errno));
    exit (1);
  }

  if (master.debug & DEBUG_CONFIG)
    printf ("create log on %d '%s'\n", fd, path);
  int sz = READ_SZ;
  hin_buffer_t * buf = malloc (sizeof (hin_buffer_t) + sz);
  memset (buf, 0, sizeof (hin_buffer_t));
  buf->flags = HIN_FILE | HIN_OFFSETS;
  buf->fd = fd;
  buf->count = 0;
  buf->sz = sz;
  buf->pos = 0;
  buf->ptr = buf->buffer;
  buf->debug = master.debug;
  buf->callback = hin_log_write_callback;

  lua_pushlightuserdata (L, buf);
  lua_pushcclosure (L, l_log, 1);

  logs = buf;

  return 1;
}

static int l_hin_redirect_log (lua_State *L) {
  int type;
  type = lua_type (L, 1);
  if (type == LUA_TSTRING) {
    const char * path = lua_tostring (L, 1);
    hin_redirect_log (path);
  } else if (type != LUA_TNONE && type != LUA_TNIL) {
    printf ("redirect log path needs to be a string\n");
  }

  if (master.flags & HIN_PRETEND) return 0;

  type = lua_type (L, 2);
  if (type == LUA_TSTRING) {
    const char * mask = lua_tostring (L, 2);
    char * ptr = NULL;
    uint32_t nr = strtol (mask, &ptr, 16);
    if (ptr <= mask) {
      printf ("can't parse debug mask\n");
    }
    master.debug = nr;
  } else if (type != LUA_TNONE && type != LUA_TNIL) {
    printf ("redirect log debug mask needs to be a string\n");
  }

  return 0;
}

static int l_hin_create_cert (lua_State *L) {
  const char * cert = lua_tostring (L, 1);
  const char * key = lua_tostring (L, 2);

#ifdef HIN_USE_OPENSSL
  SSL_CTX * hin_ssl_init (const char * cert, const char * key);

  SSL_CTX * ctx = NULL;
  if (cert && key) {
    ctx = hin_ssl_init (cert, key);
  }
#else
  if (cert || key) {
    printf ("ERROR! this build does not have ssl enabled\n");
  }
  void * ctx = NULL;
#endif
  if (ctx == NULL) {
    lua_pushboolean (L, 0);
    return 1;
  }
  hin_ssl_ctx_t * box = calloc (1, sizeof (*box));
  box->cert = strdup (cert);
  box->key = strdup (key);
  box->ctx = ctx;
  box->magic = HIN_CERT_MAGIC;
  box->refcount++;
  box->next = master.certs;
  master.certs = box;
  lua_pushlightuserdata (L, box);
  return 1;
}

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
    if (ctx == NULL) return -1;
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

static int l_hin_add_vhost (lua_State *L) {
  if (lua_type (L, 1) != LUA_TTABLE) {
    printf ("add_vhost requires a table\n");
    return 0;
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
  }
  lua_pop (L, 1);

  // listen to sockets
  lua_pushstring (L, "socket");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TTABLE) {
    size_t sz = hin_lua_rawlen (L, -1);
    for (int i = 1; i <= sz; i++) {
      lua_rawgeti (L, -1, i);
      if (l_hin_add_socket (L, vhost, lua_gettop (L)) >= 0) {
        nsocket++;
      }
      lua_pop (L, 1);
    }
  }
  lua_pop (L, 1);

  if (nsocket == 0) {
    if (parent == NULL) {
      printf ("error! vhost requires parent\n");
      return 0;
    } else if (parent->magic != HIN_VHOST_MAGIC) {
      printf ("error! vhost requires valid parent %x %x\n", parent->magic, HIN_VHOST_MAGIC);
      return 0;
    }
  }

  lua_pushstring (L, "host");
  lua_gettable (L, 1);
  if (lua_type (L, -1) == LUA_TTABLE) {
    size_t sz = hin_lua_rawlen (L, -1);
    for (int i = 1; i <= sz; i++) {
      lua_rawgeti (L, -1, i);
      const char * name = lua_tolstring (L, -1, &len);
      if (i == 1) {
        vhost->hostname = strdup (name);
      }
      if (master.debug & DEBUG_HTTP)
        printf ("vhost '%s'\n", name);
      if (hin_vhost_get (name, len)) {
        printf ("error! vhost duplicate '%s'\n", name);
        return 0;
      }
      int ret = hin_vhost_add (name, len, vhost);
      if (ret < 0) {
        printf ("error! vhost add '%s'\n", name);
        // TODO cancel the whole host
        return 0;
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

  vhost->request_callback = l_hin_vhost_get_callback (L, 1, "onRequest");
  if (nsocket > 0 && vhost->request_callback == 0) {
    printf ("error! missing onRequest handler\n");
    return 0;
  }
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

static lua_function_t functs [] = {
{"create_log",		l_hin_create_log },
{"redirect_log",	l_hin_redirect_log },
{"create_cert",		l_hin_create_cert },
{"add_vhost",		l_hin_add_vhost },
{NULL, NULL},
};

int hin_lua_config_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

