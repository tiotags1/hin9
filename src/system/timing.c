
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hin.h"

hin_timer_t * first = NULL, * last = NULL;

static int hin_timer_add_int (hin_timer_t * base, hin_timer_t * timer) {
  if (base == NULL) {
    timer->next = first;
    if (first) first->prev = timer;
    if (last == NULL) last = timer;
    first = timer;
    return 0;
  }
  timer->prev = base;
  timer->next = base->next;
  if (base->next)
    base->next->prev = timer;
  base->next = timer;
  if (timer->next == NULL) last = timer;
  return 0;
}

int hin_timer_remove (hin_timer_t * timer) {
  if (timer->next == NULL && timer->prev == NULL && timer != first) {
    return -1;
  }
  if (timer->next) {
    timer->next->prev = timer->prev;
  }
  if (timer->prev) {
    timer->prev->next = timer->next;
  }
  if (timer == first) {
    first = timer->next;
  }
  if (timer->next == NULL) {
    last = timer->prev;
  }
  timer->next = timer->prev = NULL;
  timer->time = 0;
  return 0;
}

int hin_timer_update (hin_timer_t * timer, time_t new) {
  if (timer->time == new) return 0;
  timer->time = new;
  hin_timer_t * base = timer;
  if (timer->next == NULL && timer->prev == NULL) {
    base = last;
  }
  for (;base && base->next && (base->time < new); base=base->next) {}
  for (;base && (base->time > new); base=base->prev) {}
  hin_timer_remove (timer);
  hin_timer_add_int (base, timer);
  return 0;
}

int hin_timer_check () {
  time_t tm = time (NULL);
  hin_timer_t * next = NULL;
  for (hin_timer_t * timer = first; timer && (timer->time < tm);) {
    if (master.debug & DEBUG_TIMEOUT)
      printf ("timeout %p\n", timer->ptr);
    next = timer->next;
    if (timer->callback (timer, tm) == 0 && timer->time < tm) {
      hin_timer_remove (timer);
      // free ?
    }
    timer = next;
  }
  return 0;
}

void hin_timer_flush () {
  for (hin_timer_t * timer = first; timer; timer=timer->next) {
    timer->time = 0;
  }
  hin_timer_check ();
}

