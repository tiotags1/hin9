
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "basic_lists.h"

int basic_dlist_prepend (basic_dlist_t * head, basic_dlist_t * new) {
  basic_dlist_t * last = new;
  while (last->next) last = last->next;

  if (head->next == NULL) {
    head->next = new;
    head->prev = last;
    return 0;
  }

  new->next = head->next;
  head->next->prev = last;
  head->next = new;
  return 0;
}

int basic_dlist_append (basic_dlist_t * head, basic_dlist_t * new) {
  basic_dlist_t * last = new;
  while (last->next) last = last->next;

  if (head->next == NULL) {
    head->next = new;
    head->prev = last;
    return 0;
  }

  new->prev = head->prev;
  head->prev->next = new;
  head->prev = last;

  return 0;
}

int basic_dlist_add_after (basic_dlist_t * list, basic_dlist_t * elem, basic_dlist_t * new) {
  basic_dlist_t * last = new;
  while (last->next) last = last->next;

  last->next = elem->next;
  if (elem->next) elem->next->prev = last;

  new->prev = elem;
  elem->next = new;

  if (list) {
    if (list->prev == elem) list->prev = last;
  }

  return 0;
}

int basic_dlist_remove (basic_dlist_t * head, basic_dlist_t * new) {
  if (head->next == new) {
    head->next = new->next;
  }
  if (head->prev == new) {
    head->prev = new->prev;
  }
  if (new->next) {
    new->next->prev = new->prev;
  }
  if (new->prev) {
    new->prev->next = new->next;
  }
  new->next = new->prev = NULL;
  return 0;
}

void * basic_dlist_ptr (basic_dlist_t * new, int off) {
  if (new == NULL) return NULL;
  uint8_t * data = (uint8_t*)new;
  data -= off;
  return (void*)data;
}



