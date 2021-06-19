
#ifndef HIN_WORKER_H
#define HIN_WORKER_H

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

