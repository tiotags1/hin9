#ifndef HIN_FCGI_H
#define HIN_FCGI_H

#include "hin.h"

#define FCGI_VERSION_1           1

#define FCGI_BEGIN_REQUEST       1
#define FCGI_ABORT_REQUEST       2
#define FCGI_END_REQUEST         3
#define FCGI_PARAMS              4
#define FCGI_STDIN               5
#define FCGI_STDOUT              6
#define FCGI_STDERR              7
#define FCGI_DATA                8
#define FCGI_GET_VALUES          9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_MAXTYPE (FCGI_UNKNOWN_TYPE)

#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3

#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       2
#define FCGI_UNKNOWN_ROLE     3

typedef struct __attribute__((__packed__)) {
  uint8_t version;
  uint8_t type;
  uint16_t request_id;
  uint16_t length;
  uint8_t padding;
  uint8_t reserved;
  uint8_t data[];
} FCGI_Header;

typedef struct __attribute__((__packed__)) {
  uint8_t version;
  uint8_t type;
  uint16_t request_id;
  uint16_t length;
  uint8_t padding;
  uint8_t reserved;
} FCGI_Header_int;

typedef struct __attribute__((__packed__)) {
  uint16_t role;
  uint8_t flags;
  uint8_t reserved[5];
} FCGI_BeginRequestBody;

typedef struct __attribute__((__packed__)) {
  FCGI_Header_int header;
  FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;

typedef struct __attribute__((__packed__)) {
  uint32_t appStatus;
  uint8_t protocolStatus;
  uint8_t reserved[3];
} FCGI_EndRequestBody;

enum { HIN_FCGI_SOCKET_REUSE = 0x1, };

typedef struct {
  int req_id;
  httpd_client_t * http;

  hin_buffer_t * header_buf;
  hin_pipe_t * out;

  struct hin_fcgi_socket_struct * socket;
} hin_fcgi_worker_t;

typedef struct hin_fcgi_socket_struct {
  int fd;
  uint32_t flags;

  struct sockaddr ai_addr;
  socklen_t ai_addrlen;

  hin_fcgi_worker_t ** worker;
  int num_worker, max_worker;

  hin_fcgi_worker_t * queued;

  struct hin_fcgi_group_struct * fcgi;
} hin_fcgi_socket_t;

typedef struct hin_fcgi_group_struct {
  char * host;
  char * port;
  char * uri;

  hin_fcgi_worker_t * free, * busy;

  uint32_t magic;

  struct hin_fcgi_group_struct * next;
} hin_fcgi_group_t;

FCGI_Header * hin_fcgi_header (hin_buffer_t * buf, int type, int id, int sz);
int hin_fcgi_write_request (hin_fcgi_worker_t * worker);

hin_fcgi_socket_t * hin_fcgi_get_socket (hin_fcgi_group_t * fcgi);
void hin_fcgi_socket_close (hin_fcgi_socket_t * socket);

hin_fcgi_worker_t * hin_fcgi_get_worker (hin_fcgi_group_t * fcgi_group);
int hin_fcgi_worker_reset (hin_fcgi_worker_t * worker);
void hin_fcgi_worker_run (hin_fcgi_worker_t * worker);
void hin_fcgi_worker_free (hin_fcgi_worker_t * worker);

#endif

