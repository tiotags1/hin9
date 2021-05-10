
#ifndef BASIC_VFS_INT_H
#define BASIC_VFS_INT_H

#include "basic_vfs.h"

int basic_vfs_stat_dir (basic_vfs_t * vfs, basic_vfs_dir_t * dir, const char * path, int path_len);
int basic_vfs_node_free (basic_vfs_node_t * node);
int basic_vfs_add_inotify (basic_vfs_t * vfs, basic_vfs_dir_t * dir);

#endif


