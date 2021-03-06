
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "uri.h"
#include "file.h"
#include "system/hin_lua.h"

static int l_hin_parse_path (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  httpd_client_t * http = (httpd_client_t*)client;

  if (http->headers.ptr == NULL) return 0;

  string_t temp = http->headers;
  string_t line, path;
  int method, version;
  if (hin_find_line (&temp, &line) == 0 || hin_http_parse_header_line (&line, &method, &path, &version) < 0) {
    printf ("error! %d\n", 678768312);
    return 0;
  }

  hin_uri_t uri;
  memset (&uri, 0, sizeof uri);
  if (hin_parse_uri (path.ptr, path.len, &uri) < 0) {
    printf ("error! parsing uri\n");
    return 0;
  }

  line = uri.path;
  char * parsed = hin_parse_url_encoding (&line, 0);

  lua_pushstring (L, parsed);
  lua_pushlstring (L, uri.query.ptr, uri.query.len);
  lua_pushstring (L, hin_http_method_name (method));
  lua_pushinteger (L, version);
  lua_pushstring (L, http->hostname);

  free (parsed);

  return 5;
}

static int l_hin_parse_headers (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  httpd_client_t * http = (httpd_client_t*)client;
  string_t temp = http->headers;
  string_t line, param;
  if (hin_find_line (&temp, &line) == 0) {
    return 0;
  }

  lua_newtable (L);
  int n = 1;

  while (hin_find_line (&temp, &line)) {
    if (line.len == 0) break;
    int used = match_string (&line, "([%w%-_]+):%s*", &param);
    if (used <= 0) {
      lua_pushnumber (L, n++);
      lua_pushlstring (L, line.ptr, line.len);
      lua_settable (L, -3);
      continue;
    }
    lua_pushlstring (L, param.ptr, param.len);
    lua_gettable (L, 2);
    switch (lua_type (L, -1)) {
    case LUA_TNIL:
      lua_pop (L, 1);

      lua_pushlstring (L, param.ptr, param.len);
      lua_pushlstring (L, line.ptr, line.len);
      lua_settable (L, -3);
    break;
    case LUA_TSTRING:
      lua_newtable (L);

      lua_pushnumber (L, 1);
      lua_pushvalue (L, -3);
      lua_settable (L, -3);

      lua_pushnumber (L, 2);
      lua_pushlstring (L, line.ptr, line.len);
      lua_settable (L, -3);

      lua_pushlstring (L, param.ptr, param.len);
      lua_pushvalue (L, -2);
      lua_settable (L, -5);

      lua_pop (L, 2);
    break;
    case LUA_TTABLE:
      lua_pushnumber (L, hin_lua_rawlen (L, -1) + 1);
      lua_pushlstring (L, line.ptr, line.len);
      lua_settable (L, -3);

      lua_pop (L, 1);
    break;
    default:
      printf ("error! %d\n", 43645645);
      lua_pop (L, 1);
    break;
    }
  }
  return 1;
}

int httpd_handle_file_request (hin_client_t * client, const char * path, off_t pos, off_t count, uintptr_t param);

static int l_hin_send_file (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  const char * path = lua_tostring (L, 2);
  off_t pos = 0, count = -1;
  if (lua_isnumber (L, 3)) pos = lua_tonumber (L, 3);
  if (lua_isnumber (L, 4)) count = lua_tonumber (L, 4);

  httpd_handle_file_request (client, path, pos, count, 0);

  lua_pushboolean (L, 1);
  return 1;
}

static int l_hin_proxy (lua_State *L) {
  httpd_client_t *http = (httpd_client_t*)lua_touserdata (L, 1);
  if (http == NULL || http->c.magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  #ifdef HIN_USE_RPROXY
  const char * url = lua_tostring (L, 2);
  if (url == NULL) { printf ("no path supplied\n"); return 0; }

  hin_proxy (http, NULL, url);
  #else
  httpd_error (http, 500, "no rproxy compiled");
  #endif

  lua_pushboolean (L, 1);
  return 1;
}

static int l_hin_cgi (lua_State *L) {
  httpd_client_t *http = (httpd_client_t*)lua_touserdata (L, 1);
  if (http == NULL || http->c.magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  #ifdef HIN_USE_CGI
  const char * exe_path = lua_tostring (L, 2);
  const char * root_path = lua_tostring (L, 3);
  const char * script_path = lua_tostring (L, 4);
  const char * path_info = lua_tostring (L, 5);
  if (exe_path == NULL) return 0;

  if (hin_cgi (http, exe_path, root_path, script_path, path_info) < 0) {
  }
  #else
  httpd_error (http, 500, "no cgi compiled");
  #endif
  lua_pushboolean (L, 1);
  return 1;
}

static int l_hin_fastcgi (lua_State *L) {
  httpd_client_t *http = (httpd_client_t*)lua_touserdata (L, 1);
  if (http == NULL || http->c.magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }

  #ifdef HIN_USE_FCGI
  void * group = lua_touserdata (L, 2);
  const char * script_path = lua_tostring (L, 3);
  const char * path_info = lua_tostring (L, 4);

  if (hin_fastcgi (http, group, script_path, path_info) < 0) {
  }
  #else
  httpd_error (http, 500, "no fcgi compiled");
  #endif
  lua_pushboolean (L, 1);
  return 1;
}

static int l_hin_respond (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  int status = lua_tonumber (L, 2);
  const char * body = lua_tostring (L, 3);
  httpd_client_t * http = (httpd_client_t*)client;

  httpd_respond_text (http, status, body);
  lua_pushboolean (L, 1);
  return 1;
}

static int temp_cat (char * dest, const char * source, int sz) {
  memcpy (dest, source, sz);
  dest[sz] = '\0';
  return sz;
}

static int l_hin_sanitize_path (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }

  size_t len1 = 0, len2 = 0, len3 = 1, used;

  const char * base = lua_tolstring (L, 2, &len1);
  if (base && len1 > 0) {
    if (base[len1-1] == '/') len1--;
  } else {
    base = ".";
    len1 = 1;
  }

  string_t raw, path, name;
  raw.ptr = (char*)lua_tolstring (L, 3, &raw.len);
  if (raw.ptr == NULL) return 0;
  path = raw;

  const char * index_file = "index.html";
  if (lua_isstring (L, 4)) {
    index_file = lua_tostring (L, 4);
  }

  while (1) {
    used = match_string (&path, "/([%w%.,_-]*)", &name);
    if (used <= 0) return 0;
    if (*name.ptr == '.') return 0;
    len2 += used;
    if (*path.ptr == '?') break;
    if (name.len == 0) {
      name.ptr = (char*)index_file;
      name.len = strlen (index_file);
      len3 += strlen (index_file);
      break;
    }
    if (path.len <= 0) break;
  }

  char * new = malloc (len1 + len2 + len3);
  char * ptr = new;
  ptr += temp_cat (ptr, base, len1);
  ptr += temp_cat (ptr, raw.ptr, len2);
  if (len3 > 1) {
    ptr += temp_cat (ptr, index_file, len3);
  }
  lua_pushstring (L, new);
  lua_pushlstring (L, name.ptr, name.len);

  ptr = name.ptr + name.len;
  char * min = name.ptr;
  int num = 2;
  for (;ptr>min; ptr--) {
    if (*ptr == '.') {
      ptr++;
      lua_pushlstring (L, ptr, (name.ptr + name.len)-ptr);
      num++;
      break;
    }
  }
  free (new);

  return num;
}

#include <sys/socket.h>
#include <netdb.h>

static int l_hin_remote_address (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err = getnameinfo (&client->ai_addr, client->ai_addrlen,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    lua_pushstring (L, hbuf);
    lua_pushstring (L, sbuf);
    return 2;
  } else {
    fprintf (stderr, "getnameinfo: %s\n", gai_strerror (err));
  }

  return 0;
}

static int l_hin_add_header (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  httpd_client_t * http = (httpd_client_t*)client;

  const char * name = lua_tostring (L, 2);
  const char * data = lua_tostring (L, 3);

  char * new = NULL;
  char * old = http->append_headers;
  int num = asprintf (&new, "%s: %s\r\n%s", name, data, old ? old : "");
  if (num < 0) { if (new) free (new); return -1; }

  if (old) free (old);
  http->append_headers = new;
  return 0;
}

static int l_hin_shutdown (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  httpd_client_t * http = (httpd_client_t*)client;
  http->state |= HIN_REQ_END;
  return 0;
}

static int l_hin_set_content_type (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  httpd_client_t * http = (httpd_client_t*)client;
  const char * str = lua_tostring (L, 2);
  if (str == NULL) return 0;
  if (http->content_type) free (http->content_type);
  http->content_type = strdup (str);
  return 0;
}

static int l_hin_get_vhost (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    return luaL_error (L, "requires valid client");
  }
  httpd_client_t * http = (httpd_client_t*)client;
  lua_pushlightuserdata (L, http->vhost);
  return 1;
}

int l_hin_set_path (lua_State *L);
int l_hin_list_dir (lua_State *L);

static lua_function_t functs [] = {
{"parse_path",		l_hin_parse_path },
{"parse_headers",	l_hin_parse_headers },
{"send_file",		l_hin_send_file },
{"proxy",		l_hin_proxy },
{"cgi",			l_hin_cgi },
{"fastcgi",		l_hin_fastcgi },
{"respond",		l_hin_respond },
{"sanitize_path",	l_hin_sanitize_path },
{"remote_address",	l_hin_remote_address },
{"add_header",		l_hin_add_header },
{"shutdown",		l_hin_shutdown },
{"set_content_type",	l_hin_set_content_type },
{"set_path",		l_hin_set_path },
{"list_dir",		l_hin_list_dir },
{"get_vhost",		l_hin_get_vhost },
{NULL, NULL},
};

int hin_lua_req_init (lua_State * L) {
  return lua_add_functions (L, functs);
}

