
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hin.h"
#include "http.h"
#include "lua.h"
#include "uri.h"

static int l_hin_parse_path (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_parse_path need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;

  string_t temp = http->headers;
  string_t line, method, path, param;
  if (find_line (&temp, &line) == 0 || match_string (&line, "(%a+) ("HIN_HTTP_PATH_ACCEPT") (HTTP/1.%d)", &method, &path, &param) <= 0) {
    printf ("httpd parsing error 1\n");
    return 0;
  }

  hin_uri_t uri;
  memset (&uri, 0, sizeof uri);
  if (hin_parse_uri (path.ptr, path.len, &uri) < 0) {
    printf ("error parsing uri\n");
    return 0;
  }

  lua_pushlstring (L, uri.path.ptr, uri.path.len);
  lua_pushlstring (L, uri.query.ptr, uri.query.len);
  lua_pushlstring (L, method.ptr, method.len);
  lua_pushlstring (L, param.ptr, param.len);

  return 4;
}

static int l_hin_parse_headers (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_parse_headers need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;
  string_t temp = http->headers;
  string_t line, param;
  if (find_line (&temp, &line) == 0) {
    printf ("httpd parsing error 2\n");
    return 0;
  }

  lua_newtable (L);
  int n = 1;

  while (find_line (&temp, &line)) {
    if (line.len == 0) break;
    int used = match_string (&line, "([%w%-_]+):%s*", &param);
    if (used > 0) {
      lua_pushlstring (L, param.ptr, param.len);
      lua_pushlstring (L, line.ptr, line.len);
      lua_settable (L, -3);
    } else {
      lua_pushnumber (L, n++);
      lua_pushlstring (L, line.ptr, line.len);
      lua_settable (L, -3);
    }
  }
  return 1;
}

int httpd_handle_file_request (hin_client_t * client, const char * path, off_t pos, off_t count, uintptr_t param);

static int l_hin_send_file (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_send_file need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;
  const char * path = lua_tostring (L, 2);
  if (path == NULL) { printf ("no path supplied\n"); return 0; }
  off_t pos = lua_tonumber (L, 3);
  off_t count = lua_tonumber (L, 4);
  lua_pushvalue (L, 5);
  int ref = luaL_ref (L, LUA_REGISTRYINDEX);
  httpd_handle_file_request (client, path, pos, count, ref);

  return 1;
}

static int l_hin_proxy (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_proxy need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;
  const char * url = lua_tostring (L, 2);
  if (url == NULL) { printf ("no path supplied\n"); return 0; }

  lua_pushvalue (L, 3);
  int ref = luaL_ref (L, LUA_REGISTRYINDEX);
  int hin_proxy (hin_client_t * client, const char * url);
  hin_proxy (client, url);

  return 1;
}

static int l_hin_cgi (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_cgi need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;

  const char * exe_path = lua_tostring (L, 2);
  const char * root_path = lua_tostring (L, 3);
  const char * script_path = lua_tostring (L, 4);
  if (exe_path == NULL) return 0;

  int hin_cgi (hin_client_t * client, const char * exe_path, const char * root, const char * path);
  if (hin_cgi (client, exe_path, root_path, script_path) < 0) {
    httpd_respond_fatal (http, 500, NULL);
    return 0;
  }
  return 0;
}

static int l_hin_respond (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_respond need a valid client\n");
    return 0;
  }
  int status = lua_tonumber (L, 2);
  const char * body = lua_tostring (L, 3);
  httpd_client_t * http = (httpd_client_t*)client;

  httpd_respond_text (http, status, body);
  return 0;
}

static int temp_cat (char * dest, const char * source, int sz) {
  if (sz == 0) sz = strlen (source);
  memcpy (dest, source, sz);
  dest[sz] = '\0';
  return sz;
}

static int l_hin_sanitize_path (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_sanitize_path need a valid client\n");
    return 0;
  }
  const char * base = lua_tostring (L, 2);
  if (base == NULL) { printf ("base is nil\n"); return 0; }

  int len1, len2, len3, used;
  len1 = strlen (base);
  len2 = 0;
  len3 = 1;

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
  ptr += temp_cat (ptr, base, strlen (base));
  ptr += temp_cat (ptr, raw.ptr, len2);
  if (len3 > 1) {
    ptr += temp_cat (ptr, index_file, strlen (index_file));
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
    printf ("lua hin_remote_address need a valid client\n");
    return 0;
  }

  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int err;
  err = getnameinfo (&client->in_addr, client->in_len,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
  if (err == 0) {
    lua_pushstring (L, hbuf);
    lua_pushstring (L, sbuf);
    return 2;
  } else {
    fprintf (stderr, "getnameinfo3 err '%s'\n", gai_strerror (err));
  }

  return 0;
}

static int l_hin_add_header (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_add_header need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;

  const char * name = lua_tostring (L, 2);
  const char * data = lua_tostring (L, 3);

  char * new = NULL;
  char * old = http->append_headers;
  int num = asprintf (&new, "%s: %s\r\n%s", name, data, old ? old : "");

  if (old) free (old);
  http->append_headers = new;
  return 0;
}

static int l_hin_shutdown (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_add_header need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;
  http->state |= HIN_REQ_END;
  return 0;
}

static int l_hin_set_content_type (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua hin_add_header need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;
  const char * str = lua_tostring (L, 2);
  if (str == NULL) return 0;
  if (http->content_type) free (http->content_type);
  http->content_type = strdup (str);
  return 0;
}

static lua_function_t functs [] = {
{"parse_path",		l_hin_parse_path },
{"parse_headers",	l_hin_parse_headers },
{"send_file",		l_hin_send_file },
{"proxy",		l_hin_proxy },
{"cgi",			l_hin_cgi },
{"respond",		l_hin_respond },
{"sanitize_path",	l_hin_sanitize_path },
{"remote_address",	l_hin_remote_address },
{"add_header",		l_hin_add_header },
{"shutdown",		l_hin_shutdown },
{"set_content_type",	l_hin_set_content_type },
{NULL, NULL},
};

int hin_lua_req_init (lua_State * L) {
  lua_add_functions (L, functs);
}

