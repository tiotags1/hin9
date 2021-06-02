
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
    first = last = timer;
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
  return 0;
}

int hin_timer_add (hin_timer_t * timer) {
  if (timer->next || timer->prev) hin_timer_remove (timer);
  hin_timer_t * base = last;
  for (;base && (base->time > timer->time); base=base->prev) {}
  hin_timer_add_int (base, timer);
  return 0;
}

int hin_timer_check () {
  time_t tm = time (NULL);
  if (master.debug & DEBUG_TIMEOUT)
    printf ("timer %ld\n", tm);
  hin_timer_t * next = NULL;
  for (hin_timer_t * timer = first; timer && (timer->time < tm);) {
    if (master.debug & DEBUG_TIMEOUT)
      printf ("timeout %p\n", timer);
    next = timer->next;
    timer->callback (timer, tm);
    if (timer->time < tm) {
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

