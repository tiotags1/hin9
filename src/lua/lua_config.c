
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "vhost.h"
#include "system/ssl.h"
#include "system/hin_lua.h"
#include "system/system.h"

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

static int l_hin_log_callback (lua_State *L) {
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
  if (logs) {
    hin_log_flush_single (logs);
    close (logs->fd);
    hin_buffer_clean (logs);
  }
  return 0;
}

static int l_hin_create_log (lua_State *L) {
  const char * path = lua_tostring (L, 1);
  int fd = hin_open_file_and_create_path (AT_FDCWD, path, O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT, 0660);
  if (fd < 0) {
    return luaL_error (L, "create_log '%s' %s\n", path, strerror (errno));
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
  lua_pushcclosure (L, l_hin_log_callback, 1);

  logs = buf;

  return 1;
}

static int l_hin_nil_log_callback (lua_State *L) {
  return 0;
}

static int l_hin_nil_log (lua_State *L) {
  lua_pushcclosure (L, l_hin_nil_log_callback, 0);
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
    hin_vhost_set_debug (nr);
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

static int l_hin_create_fcgi (lua_State *L) {
  #ifdef HIN_USE_FCGI
  const char * uri = lua_tostring (L, 1);

  void * hin_fcgi_start (const char * uri);
  void * ptr = hin_fcgi_start (uri);

  if (ptr == NULL) {
    return luaL_error (L, "error! fcgi can't connect '%s'\n", uri);
  }

  lua_pushlightuserdata (L, ptr);
  return 1;
  #else
  return luaL_error (L, "error! fcgi not compiled\n");
  #endif
}

int l_hin_add_vhost (lua_State *L);
int l_hin_map (lua_State *L);

static lua_function_t functs [] = {
{"create_log",		l_hin_create_log },
{"nil_log",		l_hin_nil_log },
{"redirect_log",	l_hin_redirect_log },
{"create_cert",		l_hin_create_cert },
{"create_fcgi",		l_hin_create_fcgi },
{"add_vhost",		l_hin_add_vhost },
{"map",			l_hin_map },
{NULL, NULL},
};

int hin_lua_config_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

