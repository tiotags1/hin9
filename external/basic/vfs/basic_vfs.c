
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "basic_vfs.h"
#include "internal.h"

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
    case 0:
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

