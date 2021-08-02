
#ifndef BASIC_ARGS_H
#define BASIC_ARGS_H

struct basic_args_struct;

typedef int (*basic_args_callback_t) (struct basic_args_struct * args, const char * name);

typedef struct basic_args_struct {
  int argc;
  const char ** argv;
  int index;
  basic_args_callback_t callback;
} basic_args_t;

int basic_args_process (int argc, const char * argv[], basic_args_callback_t callback);
int basic_args_cmp (const char * base, ...);
const char * basic_args_get (basic_args_t * args);

#endif

