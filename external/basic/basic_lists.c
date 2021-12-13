
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "basic_lists.h"

int hin_dlist_prepend (hin_dlist_t * head, hin_dlist_t * new) {
  new->next = new->prev = NULL;
  if (head->next == NULL) {
    head->next = head->prev = new;
  } else {
    new->next = head->next;
    head->next->prev = new;
    head->next = new;
  }
  return 0;
}

int hin_dlist_append (hin_dlist_t * head, hin_dlist_t * new) {
  new->next = new->prev = NULL;
  if (head->next == NULL) {
    head->next = head->prev = new;
  } else {
    new->prev = head->prev;
    head->prev->next = new;
    head->prev = new;
  }
  return 0;
}

int hin_dlist_remove (hin_dlist_t * head, hin_dlist_t * new) {
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

void * hin_dlist_ptr (hin_dlist_t * new, int off) {
  uint8_t * data = (uint8_t*)new;
  data -= off;
  return (void*)data;
}



