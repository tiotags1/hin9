
#ifndef HIN_MASTER_H
#define HIN_MASTER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

typedef struct {
  int sockfd;
  int type;	// listen/connection, udp/tcp, etc
  int port;
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
  char * exe_path;
  int sharefd;
  int quit;
  int num_active;
  uint32_t debug;
  int done;
  //int access_fd;
  //int error_fd;
  void * servers;
  hin_client_t * server_list;
  hin_client_t * download_list;
  hin_master_share_t * share;
} hin_master_t;

extern hin_master_t master;

#endif

