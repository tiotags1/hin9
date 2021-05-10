
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "basic_vfs.h"
#include "internal.h"

basic_vfs_node_t * basic_vfs_ref_path (basic_vfs_t * vfs, basic_vfs_node_t * dir_node, string_t * path) {
  basic_vfs_node_t * next = NULL;
  string_t source, param1;
  source = *path;
  basic_vfs_node_t * current = dir_node;

  // if initial / then set current to root
  if (path->len <= 0) return NULL;
  if (match_string (&source, "/") > 0) {
    current = &vfs->rootent;
  }
  if (current == NULL || current->type != BASIC_ENT_DIR) {
    return NULL;
  }
  // try to iterate directories
  basic_vfs_dir_t * dir = basic_vfs_get_dir (vfs, current);
  while (1) {
    int used = match_string (&source, "([%w%._-]+)", &param1);
    //printf ("matching '%.*s' in %s\n", param1.len, param1.ptr, dir->path);
    if (used <= 0) return NULL;
    next = basic_vfs_search_dir (vfs, dir, param1.ptr, param1.len);
    if (next == NULL) {
      return NULL;
    }
    if (next->type != BASIC_ENT_DIR
    || match_string (&source, "/") <= 0
    || source.len == 0) {
      *path = source;
      basic_vfs_ref (vfs, next);
      return next;
    }
    current = next;
    dir = basic_vfs_get_dir (vfs, next);
    if (dir == NULL) {
      return NULL;
    }
  }
  return NULL;
}

int basic_vfs_unref_file (basic_vfs_t * vfs, basic_vfs_file_t * inode) {
  inode->i.refcount--;
  if (inode->i.refcount > 0) { return 0; }

  if (inode->fd < 0) return -1;
  if (close (inode->fd) < 0) {
    perror ("close");
  }
  inode->fd = -1;
  return 0;
}

int basic_vfs_ref_file (basic_vfs_t * vfs, basic_vfs_file_t * inode) {
  inode->i.refcount++;

  basic_vfs_file_t * file = inode;
  if (file->fd >= 0) { return 0; }

  basic_vfs_node_t * node = inode->parent;
  basic_vfs_dir_t * dir = node->parent;

  char * path = NULL;
  int ret = asprintf (&path, "%s%s", dir->path, node->name);
  if (ret < 0) return -1;
  file->fd = openat (AT_FDCWD, path, O_RDONLY | O_CLOEXEC);

  ret = 0;
  if (file->fd < 0) {
    printf ("can't open path '%s' err '%s'\n", path, strerror (errno));
    ret = -1;
  }

  return ret;
}

int basic_vfs_ref (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  if (node == NULL) return -1;
  node->refcount++;
  if (node->type == BASIC_ENT_FILE) {
    basic_vfs_ref_file (vfs, node->inode);
  }
  return 0;
}

int basic_vfs_unref (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  if (node == NULL) return -1;
  node->refcount--;
  if (node->type == BASIC_ENT_FILE)
    basic_vfs_unref_file (vfs, node->inode);
  return 0;
}

