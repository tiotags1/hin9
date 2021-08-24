
#ifndef HIN_SYSTEM_H
#define HIN_SYSTEM_H

int hin_linux_set_limits ();
int hin_open_file_and_create_path (int dirfd, const char * path, int flags, mode_t mode);

#endif

