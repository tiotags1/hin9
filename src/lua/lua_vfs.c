
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <basic_vfs.h>

#include "hin.h"
#include "lua.h"
#include "http.h"

basic_vfs_t * vfs = NULL;

int l_hin_set_path (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua set_path need a valid client\n");
    return 0;
  }
  hin_client_t * socket = client->parent;
  hin_server_data_t * server = socket->parent;

  string_t path;
  basic_vfs_node_t * cwd = server->cwd_dir;

  path.ptr = (char*)lua_tolstring (L, 2, &path.len);

  basic_vfs_node_t * node = basic_vfs_ref_path (vfs, cwd, &path);
  if (node == NULL) {
    return 0;
  }

  if (node->type == BASIC_ENT_DIR) {
    basic_vfs_dir_t * dir = basic_vfs_get_dir (vfs, node);
    for (int i=3; i <= lua_gettop (L); i++) {
      size_t name_len = 0;
      const char * name = lua_tolstring (L, i, &name_len);
      basic_vfs_node_t * new = basic_vfs_search_dir (vfs, dir, name, name_len);
      if (new && new->type == BASIC_ENT_FILE) {
        basic_vfs_unref (vfs, node);
        basic_vfs_ref (vfs, new);
        node = new;
        break;
      }
    }
    if (node->type == BASIC_ENT_DIR) {
      lua_pushlstring (L, dir->path, dir->path_len);
      return 1;
    }
  }

  basic_vfs_file_t * file = basic_vfs_get_file (vfs, node);
  if (file == NULL) return 0;

  httpd_client_t * http = (httpd_client_t*)client;
  if (http->file) {
    basic_vfs_unref (vfs, http->file);
  }
  http->file = node;

  basic_vfs_dir_t * parent = node->parent;
  lua_pushlstring (L, parent->path, parent->path_len);
  lua_pushlstring (L, node->name, node->name_len);

  int nret = 2;

  char * ext = NULL;
  char * max = node->name + node->name_len;
  for (char * ptr = max; ptr > node->name; ptr--) {
    if (*ptr == '.') { ext = ptr+1; break; }
  }
  if (ext) {
    nret++;
    lua_pushlstring (L, ext, max - ext);
  }

  if (path.len > 0) {
    lua_pushlstring (L, path.ptr, path.len);
    nret++;
  }

  return nret;
}

int hin_server_set_work_dir (hin_server_data_t * server, const char * rel_path) {
  char * abs_path = realpath (rel_path, NULL);
  if (abs_path == NULL) { perror ("cwd realpath"); return -1; }

  int fd = openat (AT_FDCWD, abs_path, O_DIRECTORY | O_CLOEXEC);
  if (server->debug & DEBUG_CONFIG) printf ("lua server cwd '%s'\n", abs_path);
  if (fd < 0) { perror ("cwd openat"); return -1; }
  if (server->cwd_path) { free (server->cwd_path); }
  if (server->cwd_fd && server->cwd_fd != AT_FDCWD) { close (server->cwd_fd); }
  server->cwd_path = abs_path;
  server->cwd_fd = fd;

  if (vfs == NULL) {
    vfs = calloc (1, sizeof (basic_vfs_t));
    if (server->debug & DEBUG_VFS)
      vfs->debug = 1;
    basic_vfs_init (vfs);
  }

  string_t path;
  path.ptr = server->cwd_path;
  path.len = strlen (path.ptr);
  basic_vfs_node_t * cwd = basic_vfs_ref_path (vfs, NULL, &path);
  if (cwd == NULL) {
    printf ("cwd couldn't be set '%.*s'\n", (int)path.len, path.ptr);
    return -1;
  }

  basic_vfs_get_dir (vfs, cwd);
  server->cwd_dir = cwd;

  return 0;
}


