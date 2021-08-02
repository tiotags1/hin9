/*
 * basic_libs, libraries used for other projects, including, pattern matching, timers and others
 * written by Alexandru C
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at: docs/LICENSE.txt
 * documentation is in the docs folder
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basic_args.h"

int basic_args_cmp (const char * base, ...) {
  va_list ap;
  va_start (ap, base);
  int ret = 0;
  while (1) {
    const char * ptr = va_arg (ap, const char *);
    if (ptr == NULL) { break; }
    if (strcmp (base, ptr) == 0) { ret = 1; break; }
  }
  va_end (ap);
  return ret;
}

const char * basic_args_get (basic_args_t * args) {
  int num = ++args->index;
  if (num >= args->argc) {
    return NULL;
  }
  return args->argv[num];
}

static int argv_process1 (basic_args_t * args) {
  const char * base = args->argv[args->index];
  const char * ptr = base;
  if (*ptr != '-') return 0;
  ptr++;
  if (*ptr == '-') {
    ptr++;
    //printf ("long option '%s'\n", ptr);
    return args->callback (args, base);
  }
  char buf[4];
  buf[0] = '-';
  buf[2] = '\0';
  while (*ptr) {
    buf[1] = *ptr;
    //printf ("option '%s'\n", buf);
    int ret = args->callback (args, buf);
    if (ret) return ret;
    ptr++;
  }
  return 0;
}

int basic_args_process (int argc, const char * argv[], basic_args_callback_t callback) {
  basic_args_t args;
  args.argc = argc;
  args.argv = argv;
  args.callback = callback;
  for (args.index = 1; args.index < args.argc; args.index++) {
    int ret = argv_process1 (&args);
    if (ret) return ret;
  }
  return 0;
}


