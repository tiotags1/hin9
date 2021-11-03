
#ifndef HIN_LISTEN_H
#define HIN_LISTEN_H

#include "define.h"

enum {HIN_FLAG_RETRY=0x1};

typedef struct hin_server_struct {
  hin_client_t c;
  int (*accept_callback) (hin_client_t * client);
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
  hin_client_t * active_client;
  void * rp_base, * rp;
} hin_server_t;

#endif

