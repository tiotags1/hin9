
#ifndef BASIC_VFS_H
#define BASIC_VFS_H

#include <time.h>

#include <basic_pattern.h>
#include <basic_hashtable.h>

enum {BASIC_ENT_DIR = 1, BASIC_ENT_FILE, BASIC_ENT_UNKNOWN};

typedef struct {
  int type;
  int refcount;
} basic_vfs_inode_t;

typedef struct {
  int type;
  int refcount;

  struct basic_vfs_dir_struct * parent;
  void * inode;

  int name_len;
  char name[];
} basic_vfs_node_t;

typedef struct basic_vfs_dir_struct {
  basic_vfs_inode_t i;

  int num, used, max;
  basic_vfs_node_t ** entries;
  struct basic_vfs_dir_struct * parent;

  const char * path;
  int path_len;
} basic_vfs_dir_t;

typedef struct {
  basic_vfs_inode_t i;

  int fd;
  void * parent;

  time_t modified;
  off_t size;
  uint64_t etag;
} basic_vfs_file_t;

typedef struct {
  basic_ht_t ht;
  int inotify_fd;
  uint32_t debug;
  basic_vfs_node_t * root;
} basic_vfs_t;

int basic_vfs_init (basic_vfs_t * vfs);
int basic_vfs_clean (basic_vfs_t * vfs);

basic_vfs_node_t * basic_vfs_ref_path (basic_vfs_t *, basic_vfs_node_t * dir, string_t * path);
int basic_vfs_ref (basic_vfs_t *, basic_vfs_node_t * node);
int basic_vfs_unref (basic_vfs_t *, basic_vfs_node_t * node);

basic_vfs_inode_t * basic_vfs_stat (basic_vfs_t *, basic_vfs_node_t *, const char * path);
basic_vfs_dir_t * basic_vfs_get_dir (basic_vfs_t *, basic_vfs_node_t *);
basic_vfs_file_t * basic_vfs_get_file (basic_vfs_t *, basic_vfs_node_t *);

basic_vfs_node_t * basic_vfs_search_dir (basic_vfs_t * vfs, basic_vfs_dir_t * dir, const char * name, int name_len);
int basic_vfs_delete (basic_vfs_t * vfs, basic_vfs_dir_t * dir, const char * name, int name_len);
basic_vfs_node_t * basic_vfs_add (basic_vfs_t * vfs, basic_vfs_dir_t * dir, int type, const char * name, int len);

int basic_vfs_event (basic_vfs_t * vfs, char * buf, int len);

#include <sys/stat.h>

#ifndef STATX_MTIME
#include <linux/stat.h>
#include <unistd.h>
#include <fcntl.h>
int statx(int dirfd, const char *restrict pathname, int flags,
                 unsigned int mask, struct statx *restrict statxbuf);
#endif

#endif


