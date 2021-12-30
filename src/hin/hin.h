
#ifndef HIN_H
#define HIN_H

#include <stdint.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#if 0
#include <basic_banned.h>
#endif

#include "define.h"
#include "listen.h"
#include "system/ssl.h"
#include "system/master.h"

enum {
DEBUG_BASIC=0x1, DEBUG_CONFIG=0x2, DEBUG_VFS=0x4, DEBUG_SOCKET=0x8,
DEBUG_URING=0x10, DEBUG_SSL=0x20, DEBUG_SYSCALL=0x40, DEBUG_MEMORY=0x80,
DEBUG_HTTP=0x100, DEBUG_CGI=0x200, DEBUG_PROXY=0x400, DEBUG_HTTP_FILTER=0x800,
DEBUG_POST=0x1000, DEBUG_CHILD=0x2000, DEBUG_CACHE=0x4000, DEBUG_TIMEOUT=0x8000,
DEBUG_RW=0x10000, DEBUG_RW_ERROR=0x20000, DEBUG_PIPE=0x40000, DEBUG_INFO=0x80000,
DEBUG_PROGRESS=0x100000, DEBUG_LAST = 0x200000,
};

typedef struct {
  int fd;
  uint32_t flags;  // flags deal with if recv or read
  hin_ssl_t * ssl;
  off_t pos;
} hin_pipe_dir_t;

struct hin_pipe_struct {
  hin_pipe_dir_t in, out;
  off_t count, left, sz;
  void * parent, * parent1;
  basic_dlist_t write_que, writing, reading;
  int (*decode_callback) (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
  int (*read_callback) (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
  int (*in_callback) (hin_pipe_t * pipe, hin_buffer_t * buffer, int num, int flush);
  int (*finish_callback) (hin_pipe_t * pipe);
  int (*out_error_callback) (hin_pipe_t * pipe);
  int (*in_error_callback) (hin_pipe_t * pipe);
  hin_buffer_t * (*buffer_callback) (hin_pipe_t * pipe, int sz);

  uint32_t flags;
  uint32_t debug;
  uint64_t hash;
  void * extra;
};

int hin_connect (const char * host, const char * port, hin_callback_t callback, void * parent, struct sockaddr * ai_addr, socklen_t * ai_addrlen);
int hin_unix_sock (const char * path, hin_callback_t callback, void * parent);

int hin_listen_request (const char * addr, const char *port, const char * sock_type, hin_server_t * client);
int hin_listen_do ();

int hin_server_start_accept (hin_server_t * server);

// uring
int hin_request_write (hin_buffer_t * buffer);
int hin_request_read (hin_buffer_t * buffer);
int hin_request_write_fixed (hin_buffer_t * buffer);
int hin_request_read_fixed (hin_buffer_t * buffer);
int hin_request_accept (hin_buffer_t * buffer, int flags);
int hin_request_connect (hin_buffer_t * buffer, struct sockaddr * ai_addr, int ai_addrlen);
int hin_request_close (hin_buffer_t * buffer);
int hin_request_openat (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mode);
int hin_request_statx (hin_buffer_t * buffer, int dfd, const char * path, int flags, int mask);
int hin_request_timeout (hin_buffer_t * buffer, struct timespec * ts, int count, int flags);
int hin_request_is_overloaded ();

void hin_client_unlink (hin_client_t * client);
void hin_client_shutdown (hin_client_t * client);
void hin_client_close (hin_client_t * client);

int hin_client_ssl_init (hin_client_t * client);
void hin_client_ssl_cleanup (hin_client_t * client);

hin_buffer_t * hin_pipe_get_buffer (hin_pipe_t * pipe, int sz);
int hin_pipe_init (hin_pipe_t * pipe);
int hin_pipe_start (hin_pipe_t * pipe);
int hin_pipe_advance (hin_pipe_t * pipe);
int hin_pipe_finish (hin_pipe_t * pipe);

int hin_pipe_append_raw (hin_pipe_t * pipe, hin_buffer_t * buffer);
int hin_pipe_prepend_raw (hin_pipe_t * pipe, hin_buffer_t * buf);
int hin_pipe_write_process (hin_pipe_t * pipe, hin_buffer_t * buffer);

hin_buffer_t * hin_buffer_create_from_data (void * parent, const char * ptr, int sz);
void hin_buffer_clean (hin_buffer_t * buffer);

void hin_buffer_list_remove (hin_buffer_t ** list, hin_buffer_t * new);
void hin_buffer_list_append (hin_buffer_t ** list, hin_buffer_t * new);
void hin_buffer_list_add (hin_buffer_t ** list, hin_buffer_t * new);

int hin_buffer_continue_write (hin_buffer_t * buf, int ret);
int hin_buffer_prepare (hin_buffer_t * buffer, int num);
int hin_buffer_eat (hin_buffer_t * buffer, int num);

#define hin_buffer_list_ptr(elem) (basic_dlist_ptr (elem, offsetof (hin_buffer_t, list)))

int hin_lines_request (hin_buffer_t * buffer, int min);
int hin_lines_reread (hin_buffer_t * buf);
int hin_lines_write (hin_buffer_t * buf, char * data, int len);
hin_buffer_t * hin_lines_create_raw (int sz);

// timing
struct hin_timer_struct;
typedef int (*hin_timer_callback_t) (struct hin_timer_struct * timer, time_t tm);

typedef struct hin_timer_struct {
  void * ptr;
  hin_timer_callback_t callback;
  time_t time;
  struct hin_timer_struct * next, * prev;
} hin_timer_t;

int hin_timer_update (hin_timer_t * timer, time_t new);
int hin_timer_remove (hin_timer_t * timer);

int hin_timer_init (int (*callback) (int ms));

#endif
