
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hin.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

// musl doesn't define statx, what is the proper way to use statx on posix ?
#ifndef STATX_MTIME
#include <linux/stat.h>
#include <sys/syscall.h>

// TODO this code works only with x86, arm64 and x86_64
// I don't know enought about kernel development to get this in a generic way
// totaly not going to get security bugs if you let every developer figure out
// kernel development on their own, musl
#if __x86_64__
#define __NR_statx 332
#else
  #if __aarch64__
    #define __NR_statx 291
  #else
    #define __NR_statx 383
  #endif
#endif

int statx(int dirfd, const char *restrict pathname, int flags,
                 unsigned int mask, struct statx *restrict statxbuf) {
  return syscall (__NR_statx, (dirfd), (pathname), (flags), (mask), (statxbuf));
}

#endif

