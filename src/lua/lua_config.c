
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

static int l_hin_create_httpd (lua_State *L) {
  int ref1 = 0, ref2 = 0, ref3 = 0;
  if (lua_type (L, 1) != LUA_TFUNCTION) {
    printf ("not provided a callback\n");
    return 0;
  }

  lua_pushvalue (L, 1);
  ref1 = luaL_ref (L, LUA_REGISTRYINDEX);

  if (lua_type (L, 2) == LUA_TFUNCTION) {
    lua_pushvalue (L, 2);
    ref2 = luaL_ref (L, LUA_REGISTRYINDEX);
  }

  if (lua_type (L, 3) == LUA_TFUNCTION) {
    lua_pushvalue (L, 3);
    ref3 = luaL_ref (L, LUA_REGISTRYINDEX);
  }

  hin_server_data_t * server = calloc (1, sizeof (hin_server_data_t));
  server->request_callback = ref1;
  server->error_callback = ref2;
  server->finish_callback = ref3;
  server->L = L;
  server->magic = HIN_SERVER_MAGIC;
  server->timeout = HIN_HTTPD_TIMEOUT;
  server->debug = master.debug;
  lua_pushlightuserdata (L, server);

  hin_server_set_work_dir (server, master.workdir_path);

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
  if (lua_type (L, 5) == LUA_TBOOLEAN) {
    printf ("ssl missing cert for '%s:%s'\n", addr, port);
    exit (1);
  }
  void * ctx = (void*)lua_topointer (L, 5);

  hin_server_t * sock = httpd_create (addr, port, type, ctx);
  sock->c.parent = server;
  lua_pushlightuserdata (L, sock);

  return 1;
}

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
  int fd = openat (AT_FDCWD, path, O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT, 0640);
  if (fd < 0) {
    printf ("can't open '%s' %s\n", path, strerror (errno));
    return 0;
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

    if (master.flags & HIN_PRETEND) path = "/dev/null";
    int fd = openat (AT_FDCWD, path, O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT, 0640);
    if (fd < 0) {
      printf ("can't open '%s' %s\n", path, strerror (errno));
      return 0;
    }

    if (master.debug & DEBUG_CONFIG)
      printf ("create log on %d '%s'\n", fd, path);

    fflush (stdout);
    fflush (stderr);
    if (dup2 (fd, STDOUT_FILENO) < 0) perror ("dup2 stdout");
    if (dup2 (fd, STDERR_FILENO) < 0) perror ("dup2 stderr");
    close (fd);
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
  if (ctx) {
    lua_pushlightuserdata (L, ctx);
  } else {
    lua_pushboolean (L, 0);
  }
  return 1;
}

static int l_hin_add_vhost (lua_State *L) {
  size_t len = 0;
  const char * name = lua_tolstring (L, 1, &len);

  // TODO check if this is an actual ssl context
  void * ctx = (void*)lua_topointer (L, 2);

  int hin_vhost_add (const char * name, int name_len, void* ptr);
  int ret = hin_vhost_add (name, len, ctx);

  lua_pushboolean (L, ret < 0 ? 0 : 1);
  return 1;
}

static lua_function_t functs [] = {
{"create_httpd",	l_hin_create_httpd },
{"listen",		l_hin_listen },
{"create_log",		l_hin_create_log },
{"redirect_log",	l_hin_redirect_log },
{"create_cert",		l_hin_create_cert },
{"add_vhost",		l_hin_add_vhost },
{NULL, NULL},
};

int hin_lua_config_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

