
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

basic_vfs_dir_t * basic_vfs_get_dir (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  if (node->type != BASIC_ENT_DIR) return NULL;
  if (node->inode) return node->inode;

  basic_vfs_dir_t * parent = node->parent;

  int new_len = 0;
  new_len += node->name_len + 1;
  new_len += parent->path_len;

  char * new_path = malloc (new_len+1);
  int pos = new_len;
  new_path[pos--] = '\0';
  new_path[pos--] = '/';
  pos++;
  pos -= node->name_len;
  memcpy (&new_path[pos], node->name, node->name_len);
  pos -= parent->path_len;
  memcpy (&new_path[pos], parent->path, parent->path_len);

  basic_vfs_dir_t * new_dir = calloc (1, sizeof (basic_vfs_dir_t));
  new_dir->parent = node->parent;
  node->inode = new_dir;

  if (basic_vfs_stat_dir (vfs, new_dir, new_path, new_len) < 0) {
    printf ("error populating dir\n");
    return NULL;
  }

  return new_dir;
}

basic_vfs_inode_t * basic_vfs_stat (basic_vfs_t * vfs, basic_vfs_node_t * node, const char * path) {
  if (node->inode) return node->inode;
  struct statx stat;

  if (statx (AT_FDCWD, path, 0, STATX_MTIME | STATX_SIZE | STATX_TYPE, &stat) < 0) {
    perror ("statx");
    return NULL;
  }

  if (stat.stx_mode & S_IFDIR) {
    node->type = BASIC_ENT_DIR;
    return NULL;
  }

  node->type = BASIC_ENT_FILE;

  uint64_t etag = 0;
  etag += stat.stx_mtime.tv_sec * 0xffff;
  etag += stat.stx_mtime.tv_nsec * 0xff;
  etag += stat.stx_size;

  basic_vfs_file_t * file = calloc (1, sizeof (basic_vfs_file_t));
  file->fd = -1;
  file->size = stat.stx_size;
  file->etag = etag;
  file->modified = stat.stx_mtime.tv_sec;
  file->i.type = 0;
  file->parent = node;

  return (basic_vfs_inode_t*)file;
}

basic_vfs_file_t * basic_vfs_get_file (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  if (node == NULL || node->type != BASIC_ENT_FILE) return NULL;
  if (node->inode) return node->inode;

  basic_vfs_dir_t * dir = node->parent;

  char * path = NULL;
  int ret = asprintf (&path, "%s/%s", dir->path, node->name);
  if (ret < 0) { return NULL; }
  node->inode = basic_vfs_stat (vfs, node, path);
  free (path);
  return node->inode;
}

