
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
  free (path);

  if (file->fd < 0) {
    printf ("can't open path '%s' err '%s'\n", path, strerror (errno));
    return -1;
  }

  return 0;
}

int basic_vfs_ref (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  if (node == NULL) return -1;
  node->refcount++;
  //printf ("ref   %p %d\n", node, node->refcount);
  if (node->type == BASIC_ENT_FILE)
    basic_vfs_ref_file (vfs, node->inode);
  return 0;
}

int basic_vfs_unref (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  if (node == NULL) return -1;
  node->refcount--;
  //printf ("unref %p %d\n", node, node->refcount);
  if (node->type == BASIC_ENT_FILE)
    basic_vfs_unref_file (vfs, node->inode);
  return 0;
}

