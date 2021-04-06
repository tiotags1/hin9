
#ifndef BASIC_TIME_H
#define BASIC_TIME_H

// time helpers
#include <stdint.h>
#include <time.h>

#include <basic_types.h>

typedef struct {
  int64_t sec;
  int32_t nsec;
} basic_time_t;

typedef struct {
  basic_time_t first_frame;
  basic_time_t past_frame;
  basic_time_t last_frame;
  basic_time_t diff_frame;
  basic_time_t per_frame, elasped;

  basic_ftime dt;

  unsigned int target_fps;
} basic_timer_t;

basic_time_t basic_time_get ();
void basic_time_delay (basic_ftime seconds);

basic_ftime basic_time_fdiff (basic_time_t * start, basic_time_t * end);
basic_time_t basic_time_diff (basic_time_t * start, basic_time_t * end);

void basic_timer_init (basic_timer_t * timer, int);
void basic_timer_update (basic_timer_t * timer);
int basic_timer_frames (basic_timer_t * timer);


#ifdef BASIC_PROFILING
#include <sys/resource.h>
typedef struct rusage rusage_t;
void print_diff_rusage (rusage_t usage1, rusage_t usage2, const char * when);
#endif


#endif

