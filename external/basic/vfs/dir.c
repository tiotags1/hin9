
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

#include "basic_vfs.h"
#include "internal.h"

basic_vfs_node_t * basic_vfs_search_dir (basic_vfs_t * vfs, basic_vfs_dir_t * dir, const char * name, int name_len) {
  for (int i=0; i < dir->num; i++) {
    basic_vfs_node_t * dent = dir->entries[i];
    if (dent == NULL) continue;
    if (dent->name_len != name_len) continue;
    if (memcmp (dent->name, name, name_len) != 0) continue;
    return dent;
  }
  return NULL;
}

int basic_vfs_delete (basic_vfs_t * vfs, basic_vfs_dir_t * dir, const char * name, int name_len) {
  for (int i=0; i < dir->num; i++) {
    basic_vfs_node_t * node = dir->entries[i];
    if (node == NULL) continue;
    if (node->name_len != name_len) continue;
    if (memcmp (node->name, name, name_len) != 0) continue;
    basic_vfs_node_free (node);
    return 1;
  }
  return 0;
}

basic_vfs_node_t * basic_vfs_add (basic_vfs_t * vfs, basic_vfs_dir_t * dir, int type, const char * name, int len) {
  basic_vfs_node_t * new = basic_vfs_search_dir (vfs, dir, name, len);
  if (new) return new;

  int id = dir->num++;
  dir->used++;
  if (dir->used < dir->num) {
    dir->num = 0;
    while (1) {
      if (dir->entries && dir->entries[id] == NULL) break;
      id++;
      if (id >= dir->num) {
        dir->num++;
        break;
      }
    }
  }
  if (dir->num >= dir->max) {
    dir->max += 20;
    dir->entries = realloc (dir->entries, dir->max * sizeof (void*));
    memset (&dir->entries[dir->max-20], 0, 20 * sizeof (void*));
  }

  new = calloc (1, sizeof (*new) + len + 1);
  new->name_len = len;
  new->parent = dir;
  new->type = type;
  new->name_len = len;
  memcpy (new->name, name, len + 1);
  dir->entries[id] = new;
  return new;
}

int basic_vfs_stat_dir (basic_vfs_t * vfs, basic_vfs_dir_t * dir, const char * path, int path_len) {
  struct dirent *dent;
  dir->path = strndup (path, path_len+1);
  dir->path_len = path_len;
  DIR * d = opendir (dir->path);
  if (vfs->debug) printf ("vfs populating '%s'\n", dir->path);
  if (d == NULL) { perror ("opendir"); printf ("can't find '%s'\n", dir->path); return -1; }

  int basic_vfs_add_inotify (basic_vfs_t * vfs, basic_vfs_dir_t * dir);
  basic_vfs_add_inotify (vfs, dir);

  while ((dent = readdir (d)) != NULL) {
    if (dent->d_name[0] == '.') continue;
    int type = 0;
    switch (dent->d_type) {
    case DT_DIR: type = BASIC_ENT_DIR; break;
    case DT_REG: type = BASIC_ENT_FILE; break;
    case DT_LNK: type = 0; break;
    default:
      type = BASIC_ENT_UNKNOWN;
      printf ("unknown file type for '%s'\n", dent->d_name);
    break;
    }
    if (type == BASIC_ENT_UNKNOWN) continue;
    basic_vfs_node_t * node = basic_vfs_add (vfs, dir, type, dent->d_name, strlen (dent->d_name));
    if (type != BASIC_ENT_DIR) {
      basic_vfs_get_file (vfs, node);
    }
  }
  closedir (d);
  return 0;
}

int basic_vfs_node_free (basic_vfs_node_t * node) {
  if (node->type == BASIC_ENT_DIR) {
    basic_vfs_dir_t * dir = node->inode;
    if (dir == NULL) return 0;
    for (int i=0; i < dir->num; i++) {
      if (dir->entries[i])
        basic_vfs_node_free (dir->entries[i]);
    }
    if (dir->entries) free (dir->entries);
    free (dir);
  }
  free (node);
  return 0;
}



