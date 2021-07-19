
#ifndef HIN_CHILD_H
#define HIN_CHILD_H

// children related
typedef struct hin_child_struct {
  int pid;
  int (*callback) (struct hin_child_struct * child, int ret);
} hin_child_t;

int hin_children_add (hin_child_t * child);

#endif

