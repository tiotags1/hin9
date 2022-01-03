
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hin.h"

#define hin_timer_list_ptr(elem) (basic_dlist_ptr (elem, offsetof (hin_timer_t, list)))

static basic_dlist_t timers = {NULL, NULL};

int hin_timer_remove (hin_timer_t * timer) {
  basic_dlist_remove (&timers, &timer->list);
  timer->time = 0;
  return 0;
}

int hin_timer_update (hin_timer_t * timer, time_t new) {
  if (timer->time == new) return 0;

  basic_dlist_t * base = NULL;
  if (timer->time < new) {
    for (base = timer->list.next; base; base = base->next) {
      hin_timer_t * timer1 = hin_timer_list_ptr (base);
      if (timer1->time >= new) break;
    }
    basic_dlist_remove (&timers, &timer->list);
    if (base) {
      basic_dlist_add_before (&timers, base, &timer->list);
    } else {
      basic_dlist_append (&timers, &timer->list);
    }
  } else {
    for (base = timer->list.prev; base; base = base->prev) {
      hin_timer_t * timer1 = hin_timer_list_ptr (base);
      if (timer1->time <= new) break;
    }
    basic_dlist_remove (&timers, &timer->list);
    if (base) {
      basic_dlist_add_after (&timers, base, &timer->list);
    } else {
      basic_dlist_prepend (&timers, &timer->list);
    }
  }
  timer->time = new;

  return 0;
}

int hin_timer_check () {
  time_t tm = time (NULL);

  basic_dlist_t * elem = timers.next;
  while (elem) {
    hin_timer_t * timer = hin_timer_list_ptr (elem);
    elem = elem->next;

    if (timer->time > tm) break;

    if (master.debug & DEBUG_TIMEOUT)
      printf ("timeout %p\n", timer->ptr);
    if (timer->callback (timer, tm) == 0 && timer->time <= tm) {
      hin_timer_remove (timer);
    }
  }

  return 0;
}

void hin_timer_flush () {
  for (basic_dlist_t * elem = timers.next; elem; elem = elem->next) {
    hin_timer_t * timer = hin_timer_list_ptr (elem);
    timer->time = 0;
  }
  hin_timer_check ();
}

