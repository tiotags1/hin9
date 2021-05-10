
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include "basic_vfs.h"
#include "internal.h"

static int basic_vfs_event_single (basic_vfs_t * vfs, const struct inotify_event *event) {
  if (vfs->debug) printf ("wd %d %x ", event->wd, event->mask);

  basic_vfs_dir_t * dir = NULL;
  basic_ht_pair_t * pair = basic_ht_get_pair (&vfs->ht, 0, event->wd);
  if (pair) {
    dir = (void*)pair->value2;
  }

  if (event->mask & (IN_CREATE | IN_MODIFY | IN_MOVED_TO)) {
    if (vfs->debug) {
      if (event->mask & IN_CREATE) { printf ("IN_CREATE: "); }
      if (event->mask & IN_MODIFY) { printf ("IN_MODIFY: "); }
      if (event->mask & IN_MOVED_TO) { printf ("IN_MOVED_TO: "); }
    }

    int len = strlen (event->name);
    int new_len = dir->path_len + len + 2;
    char * path = malloc (new_len);
    snprintf (path, new_len, "%.*s/%.*s", dir->path_len, dir->path, len, event->name);

    basic_vfs_node_t * node = basic_vfs_add (vfs, dir, 0, event->name, len);
    if (event->mask & IN_MODIFY) {
      // TODO better free
      free (node->inode);
      node->inode = NULL;
    }
    void * ret = basic_vfs_stat (vfs, node, path);
    free (path);
  }
  if (event->mask & (IN_DELETE | IN_MOVED_FROM)) {
    if (vfs->debug) {
      if (event->mask & IN_DELETE) { printf("IN_DELETE: "); }
      if (event->mask & IN_MOVED_FROM) { printf("IN_MOVED_FROM: "); }
    }

    basic_vfs_delete (vfs, dir, event->name, strlen (event->name));
  }
  if (event->mask & IN_DELETE_SELF) {
    if (vfs->debug) {
      printf("IN_DELETE_SELF: ");
    }
  }

  if (vfs->debug) {
    if (dir) {
      printf (" %s ", dir->path);
    }
    if (event->len)
      printf ("%.*s", event->len, event->name);
    printf ("\n");
  }
  return 0;
}

int basic_vfs_event (basic_vfs_t * vfs, char * buf, int len) {
  const struct inotify_event *event;

  if (len <= 0) return -1;

  for (char *ptr = buf; ptr < buf + len;
    ptr += sizeof(struct inotify_event) + event->len) {

    event = (const struct inotify_event *) ptr;
    basic_vfs_event_single (vfs, event);
  }

  return 0;
}

int basic_vfs_add_inotify (basic_vfs_t * vfs, basic_vfs_dir_t * dir) {
  if (dir == NULL) return -1;

  if (vfs->inotify_fd <= 0) {
    vfs->inotify_fd = inotify_init1 (IN_NONBLOCK);
    if (vfs->inotify_fd < 0) {
      perror ("inotify_init1");
      return -1;
    }
  }

  int watch = inotify_add_watch (vfs->inotify_fd, dir->path, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
  if (watch < 0) {
    fprintf (stderr, "vfs cannot watch '%s': %s\n", dir->path, strerror (errno));
    return -1;
  }
  basic_ht_set_pair (&vfs->ht, 0, watch, 0, (uintptr_t)dir);
  return 0;
}

