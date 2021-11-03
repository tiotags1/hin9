
#ifndef HIN_DEFINE_H
#define HIN_DEFINE_H

#include <stdint.h>

#include <sys/socket.h>

typedef struct hin_buffer_struct hin_buffer_t;
typedef struct hin_client_struct hin_client_t;
typedef struct hin_pipe_struct hin_pipe_t;

#include "system/ssl.h"

#define READ_SZ 4096
// TODO check if 16k is better

#define HIN_CONNECT_MAGIC 0xfeabc321
#define HIN_CLIENT_MAGIC 0xfeabc111
#define HIN_SERVER_MAGIC 0xfcadc123
#define HIN_VHOST_MAGIC 0xeeefcac1
#define HIN_CERT_MAGIC 0xfaaaacc
#define HIN_FCGI_MAGIC 0xeaeaeaea

enum {
HIN_DONE = 0x1, HIN_SOCKET = 0x2, HIN_FILE = 0x4, HIN_OFFSETS = 0x8,
HIN_SSL = 0x10, HIN_COUNT = 0x20, HIN_HASH = 0x40, HIN_SYNC = 0x80,
HIN_EPOLL_READ = 0x100, HIN_EPOLL_WRITE = 0x200, HIN_INACTIVE = 0x400,
};

#define HIN_EPOLL (HIN_EPOLL_READ | HIN_EPOLL_WRITE)

enum { HIN_CLIENT = 1, HIN_DYN_BUFFER, HIN_SERVER, HIN_DOWNLOAD, HIN_CACHE_OBJECT };

typedef int (*hin_callback_t) (hin_buffer_t * buffer, int ret);

typedef struct hin_buffer_struct {
  int type;
  int fd;
  uint32_t flags;
  uint32_t debug;
  hin_callback_t callback;
  off_t pos;
  int count, sz;
  void * parent;
  struct hin_buffer_struct * prev, * next, * ssl_buffer;
  char * ptr;
  hin_ssl_t * ssl;
  char buffer[];
} hin_buffer_t;

typedef struct hin_client_struct {
  int type;
  int sockfd;
  uint32_t flags;
  uint32_t magic;
  void * parent;
  struct sockaddr ai_addr;
  socklen_t ai_addrlen;
  hin_ssl_t ssl;
  struct hin_client_struct * prev, * next;
} hin_client_t;

typedef struct {
  int (*read_callback) (hin_buffer_t * buffer, int received);
  int (*eat_callback) (hin_buffer_t * buffer, int num);
  int (*close_callback) (hin_buffer_t * buffer, int ret);
  int count;
  char * base;
} hin_lines_t;

#endif

