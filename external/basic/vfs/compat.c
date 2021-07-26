
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

// musl doesn't define statx, what is the proper way to use statx on posix ?
#ifndef STATX_MTIME
#include <linux/stat.h>
#include <sys/syscall.h>

#include "basic_vfs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <asm/unistd.h>

int statx(int dirfd, const char *restrict pathname, int flags,
                 unsigned int mask, struct statx *restrict statxbuf) {
  return syscall (__NR_statx, (dirfd), (pathname), (flags), (mask), (statxbuf));
}

#endif

