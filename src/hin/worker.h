
#ifndef HIN_WORKER_H
#define HIN_WORKER_H

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

typedef struct {
  uint8_t version;
  uint8_t type;
  uint16_t request_id;
  uint16_t length;
  uint8_t padding;
  uint8_t reserved;
  uint8_t contentData[];
} hin_fcgi_record_t;

typedef struct {
} hin_worker_data_t;

typedef struct hin_worker_struct {
  struct hin_worker_struct * next, * prev;
  int id;
  int pid;
  int out_fd;
  int command_fd;
  int post_fd;
  int share_fd;
  int cgi_read_fd;
  int cgi_write_fd;
  hin_buffer_t * buf;
  int (*worker_close) (struct hin_worker_struct * worker, void * data, int ret);
  hin_worker_data_t * data;
} hin_worker_t;

typedef struct hin_work_order_struct {
  struct hin_work_order_struct * next, * prev;
  void * data;
  hin_buffer_t * buf;
  int (*worker_enter) (struct hin_work_order_struct * order, hin_worker_t * worker);
  int (*worker_close) (hin_worker_t * worker, void * data, int ret);
} hin_work_order_t;

int hin_worker_submit (hin_work_order_t * order);
hin_worker_t * hin_worker_get ();
int hin_worker_reset (hin_worker_t * worker);

// children related
typedef struct hin_child_struct {
  int pid;
  int (*callback) (struct hin_child_struct * child, int ret);
} hin_child_t;

int hin_children_add (hin_child_t * child);

#endif

