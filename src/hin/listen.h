
#ifndef HIN_LISTEN_H
#define HIN_LISTEN_H

#include "define.h"

enum {HIN_FLAG_RETRY=0x1};

typedef struct hin_server_struct {
  hin_client_t c;
  int (*accept_callback) (hin_client_t * client);
  void * (*sni_callback) (hin_client_t * client, const char * name, int name_len);
  int (*close_callback) (hin_client_t * client);
  int (*error_callback) (hin_client_t * client);
  int user_data_size;
  void * ssl_ctx;
  int accept_flags;
  int num_client;
  uint32_t flags;
  uint32_t debug;
  intptr_t ai_family, ai_protocol, ai_socktype;
  hin_buffer_t * accept_buffer;
  hin_client_t * client_list;
  void * rp_base, * rp;
} hin_server_t;


int hin_server_listen (const char * addr, const char * port, const char * sock_type, hin_server_t * ptr);

int hin_server_stop (hin_server_t * server);
int hin_server_unlink (hin_server_t * server);

#endif

