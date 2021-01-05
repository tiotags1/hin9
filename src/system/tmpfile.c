
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <hin.h>

static int hin_tmpfile_callback (hin_buffer_t * buf, int ret) {

}

int hin_tmpfile (int dirfd, const char * prefix, void * data, int (*callback) (void * data, int ret)) {
  hin_buffer_t * buf = calloc (1, sizeof (hin_buffer_t));
  //buf->flags = 0;
  //buf->fd = 0;
  buf->callback = hin_tmpfile_callback;
  //buf->count = buf->sz = 0;
  //buf->ptr = buf->buffer;
  // O_EXCL with O_TMPFILE does something different
  hin_request_openat (buf, dirfd, "hi", O_RDWR | O_CREAT | O_TRUNC | O_TMPFILE, 0600);
}



