
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "basic_vfs.h"
#include "internal.h"

int basic_vfs_init (basic_vfs_t * vfs) {
  basic_ht_init (&vfs->ht, 1024, 0);

  basic_vfs_node_t * root = calloc (1, sizeof (basic_vfs_node_t));
  root->type = BASIC_ENT_DIR;
  root->inode = calloc (1, sizeof (basic_vfs_dir_t));

  basic_vfs_stat_dir (vfs, root->inode, "/", 1);
  vfs->root = root;

  char * cwd = realpath (".", NULL);
  string_t path;
  path.ptr = cwd;
  path.len = strlen (path.ptr);
  basic_vfs_ref_path (vfs, NULL, &path);
  free (cwd);

  return 0;
}

int basic_vfs_node_free (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  switch (node->type) {
  case 0:
  case BASIC_ENT_FILE:
    if (node->inode == NULL) break;
    basic_vfs_unref (vfs, node);
    free (node->inode);
  break;
  case BASIC_ENT_DIR:
    if (node->inode == NULL) break;
    basic_vfs_dir_t * dir = node->inode;
    for (int i=0; i < dir->max; i++) {
      if (dir->entries[i])
        basic_vfs_node_free (vfs, dir->entries[i]);
    }
    if (dir->entries) free (dir->entries);
    if (dir->path) free ((char*)dir->path);
    free (dir);
  break;
  default:
    printf ("vfs clean unknown type ? %d\n", node->type);
  break;
  }
  free (node);
  return 0;
}

int basic_vfs_clean (basic_vfs_t * vfs) {
  basic_ht_clean (&vfs->ht);
  basic_vfs_node_free (vfs, vfs->root);
  if (vfs->inotify_fd) close (vfs->inotify_fd);
  return 0;
}

