
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "basic_vfs.h"
#include "internal.h"

/*
freeing resources by refcounting
match large path on real path
switch to epoll

ref dirs prevents them from being deleted
ref files prevents both them and the parent from being deleted

get path also gives a ref

*/

basic_vfs_node_t * basic_vfs_ref_path (basic_vfs_t * vfs, basic_vfs_node_t * dir_node, string_t * path) {
  basic_vfs_node_t * node = NULL, * next = NULL;
  string_t source, param1;
  source = *path;
  basic_vfs_node_t * current = dir_node;
  if (current == NULL) {
    current = &vfs->rootent;
  }
  if (current->type != BASIC_ENT_DIR) {
    return NULL;
  }
  if (path->len == 1 && *path->ptr == '/') {
    path->len--;
    path->ptr++;
    basic_vfs_ref (vfs, current);
    return current;
  }
  basic_vfs_dir_t * dir = basic_vfs_get_dir (vfs, current);
  while (1) {
    int used = match_string (&source, "/([%w%._-]*)", &param1);
    if (used <= 0) return NULL;
    if (param1.ptr[0] == '.') { return NULL; }
    if (used == 1 && source.len == 0) { break; }
    next = basic_vfs_search_dir (vfs, dir, param1.ptr, param1.len);
    if (next == NULL) {
      printf ("can't find path '%.*s' '%.*s'\n", (int)param1.len, param1.ptr, (int)path->len, path->ptr);
      return NULL;
    }
    if (next->type != BASIC_ENT_DIR) {
      *path = source;
      basic_vfs_ref (vfs, next);
      return next;
    }
    current = next;
    if (source.len == 0) { break; }
    dir = basic_vfs_get_dir (vfs, next);
    if (dir == NULL) return NULL;
  }
  basic_vfs_ref (vfs, current);
  return current;
}

int basic_vfs_init (basic_vfs_t * vfs) {
  basic_ht_init (&vfs->ht, 1024, 0);

  vfs->root = calloc (1, sizeof (basic_vfs_dir_t));
  vfs->rootent.inode = vfs->root;
  vfs->rootent.type = BASIC_ENT_DIR;

  basic_vfs_stat_dir (vfs, vfs->root, "/", 1);

  char * cwd = realpath (".", NULL);
  string_t path;
  path.ptr = cwd;
  path.len = strlen (path.ptr);
  basic_vfs_ref_path (vfs, NULL, &path);
  free (cwd);

  return 0;
}

static void basic_vfs_clean_dir (basic_vfs_t * vfs, basic_vfs_dir_t * dir) {
  for (int i=0; i < dir->max; i++) {
    basic_vfs_node_t * node = dir->entries[i];
    if (node == NULL) continue;
    switch (node->type) {
    case BASIC_ENT_FILE:
      if (node->inode == NULL) break;
      basic_vfs_unref (vfs, node);
      free (node->inode);
    break;
    case BASIC_ENT_DIR:
      if (node->inode == NULL) break;
      basic_vfs_clean_dir (vfs, node->inode);
    break;
    default:
      printf ("vfs clean unknown type ? %d\n", node->type);
    }
  }
  if (dir->entries) free (dir->entries);
  free (dir);
}

int basic_vfs_clean (basic_vfs_t * vfs) {
  basic_ht_clean (&vfs->ht);
  basic_vfs_clean_dir (vfs, vfs->root);
  if (vfs->inotify_fd) close (vfs->inotify_fd);
  return 0;
}

