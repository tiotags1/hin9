
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "hin.h"

char * hin_directory_path (const char * old, const char ** replace) {
  if (replace && *replace) {
    free ((void*)*replace);
  }
  int len = strlen (old);
  char * new = NULL;
  if (old[len-1] == '/') {
    new = strdup (old);
    goto done;
  }
  new = malloc (len + 2);
  memcpy (new, old, len);
  new[len] = '/';
  new[len+1] = '\0';
done:
  if (replace) {
    *replace = new;
  }
  return new;
}

int hin_open_file_and_create_path (int dirfd, const char * path, int flags, mode_t mode) {
  int fd = openat (dirfd, path, flags, mode);
  if (fd >= 0) return fd;
  if (errno != ENOENT) return fd;
  if ((master.flags & HIN_CREATE_DIRECTORY) == 0) return fd;

  mode_t mask = 0;
  if (mode & 0600) mask |= 0100;
  if (mode & 060) mask |= 010;
  if (mode & 06) mask |= 01;

  const char * ptr = path;
  while (1) {
    if (*ptr == '/') {
      ptr++;
      continue;
    }
    ptr = strchr (ptr, '/');
    if (ptr == NULL) return openat (dirfd, path, flags, mode);
    char * dir_path = strndup (path, ptr-path);
    int err = mkdirat (dirfd, dir_path, mode | mask);
    if (err == 0) {
      if (master.debug & DEBUG_CONFIG) {
        printf ("created for '%s' directory '%s'\n", path, dir_path);
      }
      free (dir_path);
      continue;
    }
    free (dir_path);

    switch (errno) {
    case ENOENT: break;
    case EEXIST: break;
    default:
      return -1;
    break;
    }
  }
}


