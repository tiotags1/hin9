
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <basic_vfs.h>

#include "hin.h"
#include "conf.h"
#include "http.h"
#include "vhost.h"

#include "system/hin_lua.h"

basic_vfs_t * vfs = NULL;

int l_hin_set_path (lua_State *L) {
  hin_client_t *client = (hin_client_t*)lua_touserdata (L, 1);
  if (client == NULL || client->magic != HIN_CLIENT_MAGIC) {
    printf ("lua set_path need a valid client\n");
    return 0;
  }
  httpd_client_t * http = (httpd_client_t*)client;
  hin_vhost_t * vhost = http->vhost;

  string_t path;
  basic_vfs_node_t * cwd = vhost->cwd_dir;

  path.ptr = (char*)lua_tolstring (L, 2, &path.len);
  match_string (&path, "/");

  basic_vfs_node_t * node = NULL;
  if (path.len > 0) {
    node = basic_vfs_ref_path (vfs, cwd, &path);
  } else {
    node = cwd;
  }
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

int hin_epoll_request_read (hin_buffer_t * buf);

static int hin_vfs_event_callback (hin_buffer_t * buf, int ret) {
  if (ret < 0) {
    printf ("inotify error '%s'\n", strerror (-ret));
    return -1;
  }
  basic_vfs_event (vfs, buf->ptr, ret);

  hin_request_read (buf);
  return 0;
}

static hin_buffer_t * inotify_buffer = NULL;

static int hin_vfs_event_init (int inotify_fd) {
  static int inited = 0;
  if (inited) return 0;
  inited = 1;
  hin_buffer_t * buf = malloc (sizeof (*buf) + READ_SZ);
  memset (buf, 0, sizeof (*buf));
  #ifdef HIN_LINUX_URING_DONT_HAVE_NOTIFYFD
  buf->flags = HIN_EPOLL;
  #endif
  buf->fd = inotify_fd;
  buf->callback = hin_vfs_event_callback;
  buf->count = buf->sz = READ_SZ;
  buf->ptr = buf->buffer;
  buf->debug = master.debug;
  hin_request_read (buf);
  inotify_buffer = buf;
  return 1;
}

int hin_vfs_clean () {
  if (vfs) {
    basic_vfs_clean (vfs);
    free (vfs);
    vfs = NULL;
  }
  if (inotify_buffer) hin_buffer_clean (inotify_buffer);
  return 0;
}

int hin_server_set_work_dir (hin_vhost_t * server, const char * rel_path) {
  char * abs_path = realpath (rel_path, NULL);
  if (abs_path == NULL) {
    fprintf (stderr, "cwd realpath '%s' %s\n", rel_path, strerror (errno));
    return -1;
  }

  int fd = openat (AT_FDCWD, abs_path, O_DIRECTORY | O_CLOEXEC);
  if (server->debug & DEBUG_CONFIG) printf ("lua server cwd '%s'\n", abs_path);
  if (fd < 0) { perror ("cwd openat"); return -1; }
  if (server->cwd_fd && server->cwd_fd != AT_FDCWD) { close (server->cwd_fd); }
  server->cwd_fd = fd;

  if (vfs == NULL) {
    vfs = calloc (1, sizeof (basic_vfs_t));
    if (server->debug & DEBUG_VFS)
      vfs->debug = 1;
    basic_vfs_init (vfs);
  }

  string_t path;
  path.ptr = abs_path;
  path.len = strlen (path.ptr);
  basic_vfs_node_t * cwd = basic_vfs_ref_path (vfs, NULL, &path);
  if (cwd == NULL) {
    printf ("cwd couldn't be set '%.*s'\n", (int)path.len, path.ptr);
    return -1;
  }
  free (abs_path);

  basic_vfs_get_dir (vfs, cwd);
  server->cwd_dir = cwd;

  if (vfs->inotify_fd > 0) {
    hin_vfs_event_init (vfs->inotify_fd);
  }

  return 0;
}


