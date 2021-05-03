
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include "basic_vfs.h"
#include "internal.h"

/*
#include <poll.h>
static int basic_vfs_event_single (basic_vfs_t * vfs, const struct inotify_event *event) {
  printf ("wd %d %x ", event->wd, event->mask);

  basic_vfs_dir_t * dir = NULL;
  basic_ht_pair_t * pair = basic_ht_get_pair (&vfs->ht, 0, event->wd);
  if (pair) {
    dir = (void*)pair->value2;
  }

  if (event->mask & IN_CREATE) {
    printf("IN_CREATE: ");
    if (event->name[0] == '.') return 0;

    int len = dir->path_len + event->len + 2;
    char * path = malloc (len);
    snprintf (path, len, "%.*s/%.*s", dir->path_len, dir->path, event->len, event->name);

    struct stat sb;
    if (stat (path, &sb) < 0) {
      perror("stat");
      return -1;
    }
    free (path);
    int type = 0;
    switch (sb.st_mode & S_IFMT) {
    case S_IFDIR: type = BASIC_ENT_DIR; break;
    case S_IFREG: type = BASIC_ENT_FILE; break;
    }

    basic_vfs_add_ent (vfs, dir, type, event->name, event->len);
  }
  if (event->mask & IN_DELETE) {
    printf("IN_DELETE: ");
    basic_vfs_delete (dir, event->name, event->len);
  }
  if (event->mask & IN_DELETE_SELF)
    printf("IN_DELETE_SELF: ");

  if (dir) {
    printf (" %s ", dir->path);
  }

  if (event->len)
    printf ("%.*s", event->len, event->name);
  printf ("\n");
}

static int basic_vfs_event (basic_vfs_t * vfs, int fd) {
  char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
  const struct inotify_event *event;
  ssize_t len;

  for (;;) {
    len = read (fd, buf, sizeof(buf));
    if (len == -1 && errno != EAGAIN) {
      perror("read");
      return -1;
    }

    if (len <= 0) break;

    for (char *ptr = buf; ptr < buf + len;
      ptr += sizeof(struct inotify_event) + event->len) {

      event = (const struct inotify_event *) ptr;
      basic_vfs_event_single (vfs, event);
    }
  }
}
*/

int basic_vfs_add_inotify (basic_vfs_t * vfs, basic_vfs_node_t * node) {
  basic_vfs_dir_t * dir = basic_vfs_get_dir (vfs, node);
  if (dir == NULL) return -1;

  if (vfs->inotify_fd <= 0) {
    vfs->inotify_fd = inotify_init1 (IN_NONBLOCK);
    if (vfs->inotify_fd < 0) {
      perror ("inotify_init1");
      return -1;
    }
  }

  int watch = inotify_add_watch (vfs->inotify_fd, dir->path, IN_CREATE | IN_DELETE);
  if (watch < 0) {
    fprintf (stderr, "vfs cannot watch '%s': %s\n", dir->path, strerror (errno));
    return -1;
  }
  basic_ht_set_pair (&vfs->ht, 0, watch, 0, (uintptr_t)dir);
  return 0;
}

