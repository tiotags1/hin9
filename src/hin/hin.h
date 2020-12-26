
#ifndef HIN_H
#define HIN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

typedef struct hin_buffer_struct hin_buffer_t;
typedef struct hin_client_struct hin_client_t;
typedef struct hin_pipe_struct hin_pipe_t;

#include "ssl.h"
#include "master.h"
#include "http.h"

#define READ_SZ                 4096
//65536

#define HIN_CONNECT_MAGIC 0xfeabc321
#define HIN_CLIENT_MAGIC 0xfeabc111
#define HIN_SERVER_MAGIC 0xfcadc123

enum { HIN_DONE = 0x1, HIN_SOCKET = 0x2, HIN_OFFSETS = 0x4, HIN_SSL = 0x8 };

enum { HIN_CLIENT = 1, HIN_DYN_BUFFER, HIN_SERVER, HIN_DOWNLOAD };

enum { DEBUG_OTHER = 0x1, DEBUG_PIPE = 0x2, DEBUG_HEADERS = 0x4, DEBUG_RW = 0x8,
  DEBUG_SSL = 0x10, DEBUG_PROTO = 0x20, DEBUG_URING = 0x40, DEBUG_SOCKET = 0x80,
  DEBUG_CGI = 0x100, DEBUG_POST = 0x200, DEBUG_CONFIG = 0x400 };

typedef int (*hin_callback_t) (hin_buffer_t * buffer, int ret);

struct hin_buffer_struct {
  int type;
  int fd;
  uint32_t flags;
  hin_callback_t callback;
  hin_callback_t error_callback;
  off_t pos;
  int count, sz;
  hin_client_t * parent;
  struct hin_buffer_struct * prev, * next;
  char * ptr, * data;
  hin_ssl_t * ssl;
  char buffer[];
};

typedef struct {
  int fd;
  uint32_t flags;  // flags deal with if recv or read
  off_t pos;
} hin_pipe_dir_t;

struct hin_pipe_struct {
  hin_pipe_dir_t in, out;
  uint32_t flags;
  off_t count, sz;
  void * parent, * data;
  hin_buffer_t * write;
  hin_ssl_t * ssl;
  int (*read_callback) (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
  int (*finish_callback) (hin_pipe_t * pipe);
  int (*extra_callback) (hin_pipe_t * pipe);
  int (*out_error_callback) (hin_pipe_t * pipe);
  int (*in_error_callback) (hin_pipe_t * pipe);
};

struct hin_client_struct {
  int type;
  int sockfd;
  uint32_t flags;
  uint32_t magic;
  void * parent;
  hin_buffer_t * read_buffer;
  struct sockaddr in_addr;
  socklen_t in_len;
  hin_ssl_t ssl;
  struct hin_client_struct * prev, * next;
  char extra[];
};

typedef struct hin_server_struct {
  int (*client_handle) (hin_client_t * client);
  int (*client_close) (hin_client_t * client);
  int (*client_error) (hin_client_t * client);
  int user_data_size;
  void * ssl_ctx;
  int accept_flags;
  hin_buffer_t * accept_buffer;
  hin_client_t * accept_client;
  hin_client_t * active_client;
  char extra[];
} hin_server_blueprint_t;

hin_client_t * hin_connect (const char * host, const char * port, int extra_size, int (*callback) (hin_client_t * client, int ret));
int hin_socket_listen (const char * address, const char * port, hin_client_t * client);

void hin_client_shutdown (hin_client_t * client);
void hin_client_close (hin_client_t * client);

// uring
int hin_request_read (hin_buffer_t * buffer);
int hin_request_write (hin_buffer_t * buffer);
int hin_request_accept (hin_buffer_t * buffer, int flags);
int hin_request_connect (hin_buffer_t * buffer);
int hin_request_close (hin_buffer_t * buffer);
int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode);
int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask);

hin_buffer_t * hin_pipe_buffer_get (hin_pipe_t * pipe);
int hin_pipe_advance (hin_pipe_t * pipe);

void hin_buffer_clean (hin_buffer_t * buffer);

void hin_buffer_list_remove (hin_buffer_t ** list, hin_buffer_t * new);
void hin_buffer_list_append (hin_buffer_t ** list, hin_buffer_t * new);
int hin_pipe_write (hin_pipe_t * client, hin_buffer_t * buffer);

void hin_client_list_remove (hin_client_t ** list, hin_client_t * new);
void hin_client_list_add (hin_client_t ** list, hin_client_t * new);

int hin_buffer_prepare (hin_buffer_t * buffer, int num);
int hin_buffer_eat (hin_buffer_t * buffer, int num);

int hin_lines_request (hin_buffer_t * buffer);
int hin_lines_reread (hin_client_t * client);
#include <basic_pattern.h>
hin_buffer_t * hin_lines_create (hin_client_t * client, int sockfd, int (*callback) (hin_client_t * client, string_t * source));

#endif

