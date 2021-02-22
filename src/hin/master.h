
#ifndef HIN_MASTER_H
#define HIN_MASTER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

typedef struct {
  int sockfd;
  int type;	// listen/connection, udp/tcp, etc
  struct sockaddr in_addr;
  socklen_t in_len;
} hin_master_socket_t;

typedef struct {
  int fd;	// sharefd;
  int done;	// 1 for finished start, -1 for error
  int nsocket;
  hin_master_socket_t sockets[];
} hin_master_share_t;

typedef struct {
  int id;
  uint32_t debug;
  char * exe_path;
  int sharefd;
  int quit;
  int restart_pid;
  int restarting;
  void * servers;
  int num_client;
  int num_connection;
  hin_client_t * server_list;
  hin_client_t * connection_list;
  hin_master_share_t * share;
} hin_master_t;

extern hin_master_t master;

void hin_stop ();
int hin_restart ();
int hin_check_alive ();

#endif

