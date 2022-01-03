#ifndef BASIC_LISTS_H
#define BASIC_LISTS_H

#include <stdint.h>

typedef struct basic_dlist_struct {
  struct basic_dlist_struct * prev, * next;
} basic_dlist_t;

int basic_dlist_prepend (basic_dlist_t * head, basic_dlist_t * new);
int basic_dlist_append (basic_dlist_t * head, basic_dlist_t * new);
int basic_dlist_remove (basic_dlist_t * head, basic_dlist_t * new);
void * basic_dlist_ptr (basic_dlist_t * new, int off);

int basic_dlist_add_after (basic_dlist_t * list, basic_dlist_t * elem, basic_dlist_t * new);
int basic_dlist_add_before (basic_dlist_t * list, basic_dlist_t * elem, basic_dlist_t * new);

#endif
