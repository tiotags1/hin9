
#ifndef HIN_MASTER_H
#define HIN_MASTER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

enum {
HIN_QUIT = 0x1, HIN_DAEMONIZE = 0x2, HIN_PRETEND = 0x4, HIN_RESTARTING = 0x8,
HIN_CREATE_DIRECTORY = 0x10,
};

typedef struct hin_master_sock_struct {
  int sockfd;
  int type;	// listen/connection, udp/tcp, etc
  intptr_t ai_family, ai_protocol, ai_socktype;
  struct sockaddr ai_addr;
  socklen_t ai_addrlen;
  void * server;
  struct hin_master_sock_struct * next;
} hin_master_socket1_t;

typedef struct {
  int sockfd;
  struct sockaddr ai_addr;
  socklen_t ai_addrlen;
} hin_master_socket_t;

typedef struct {
  int fd;	// sharefd;
  int done;	// 1 for finished start, -1 for error
  int nsocket;
  hin_master_socket_t sockets[];
} hin_master_share_t;

typedef struct {
  int id;
  int quit;
  uint32_t flags;
  uint32_t debug;
  const char * exe_path;
  const char * conf_path;
  const char * logdir_path;
  const char * tmpdir_path;
  const char * workdir_path;
  const char * pid_path;
  const char ** argv;
  const char ** envp;
  int restart_pid;
  int num_client;
  int num_connection;
  hin_master_socket1_t * socket, * last_socket;
  hin_client_t * server_list;
  hin_client_t * connection_list;
  void * vhosts;
  void * certs;
  int sharefd;
  hin_master_share_t * share;
} hin_master_t;

extern hin_master_t master;

void hin_stop ();
int hin_restart1 ();
int hin_restart2 ();
int hin_check_alive ();

char * hin_directory_path (const char * old, const char ** replace);
int hin_redirect_log (const char * path);

#endif

