#ifndef BASIC_LISTS_H
#define BASIC_LISTS_H

#include <stdint.h>

typedef struct hin_dlist_struct {
  struct hin_dlist_struct * prev, * next;
} hin_dlist_t;

int hin_dlist_prepend (hin_dlist_t * head, hin_dlist_t * new);
int hin_dlist_append (hin_dlist_t * head, hin_dlist_t * new);
int hin_dlist_remove (hin_dlist_t * head, hin_dlist_t * new);
void * hin_dlist_ptr (hin_dlist_t * new, int off);

#endif
