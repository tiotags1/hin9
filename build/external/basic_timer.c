/*
 * basic_libs, libraries used for other projects, including, pattern matching, timers and others
 * written by Alexandru C
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at: docs/LICENSE.txt
 * documentation is in the docs folder
 */

#include <stdio.h>
#include <string.h>

#include "basic_timer.h"

#define NSEC 1000000000

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
basic_time_t basic_time_get () {
  basic_time_t res;

  struct timespec time_now;
  clock_gettime (CLOCK_MONOTONIC, &time_now);
  res.sec = time_now.tv_sec;
  res.nsec = time_now.tv_nsec;
  return res;
}

void basic_time_delay (basic_float seconds) {
  unsigned int sleep_time = (unsigned int)(seconds * (float)NSEC);
  //printf ("sleeping for %d\n",sleep_time);
  usleep (sleep_time);
}
#else

#include <windows.h>

double PCFreq1 = 0.0;
double PCFreq2 = 0.0;
__int64 CounterStart = 0;

void basic_time_delay (basic_ftime seconds) {
  unsigned int sleep_time = (unsigned int)(seconds*1000);
  //printf("sleep for %d miliseconds\n", sleep_time);
  Sleep (sleep_time);
}

basic_time_t basic_time_get () {
  basic_time_t res;
  LARGE_INTEGER li;

  if (PCFreq1 == 0.0) {
    if(!QueryPerformanceFrequency(&li)) {
      basic_error ("QueryPerformanceFrequency failed!\n");
    }

    PCFreq1 = (double)(li.QuadPart);
    PCFreq2 = (double)(li.QuadPart)/1000000000.0;

    QueryPerformanceCounter(&li);
    CounterStart = li.QuadPart;
  }

  QueryPerformanceCounter(&li);

  res.sec = (li.QuadPart-CounterStart)/PCFreq1;
  res.nsec = ((li.QuadPart-CounterStart-res.sec * PCFreq1)/(PCFreq2));

  return res;
}
#endif

basic_ftime basic_time_float (basic_time_t * time) {
  basic_ftime dt = (basic_ftime)time->sec + (basic_ftime)time->nsec / (basic_ftime)NSEC;
  return dt;
}

basic_time_t basic_time_diff (basic_time_t * start, basic_time_t * end) {
  basic_time_t diff;
  if ((end->nsec - start->nsec) < 0) {
    diff.sec = end->sec - start->sec - 1;
    diff.nsec = NSEC + end->nsec - start->nsec;
  } else {
    diff.sec = end->sec - start->sec;
    diff.nsec = end->nsec - start->nsec;
  }
  return diff;
}

basic_ftime basic_time_fdiff (basic_time_t * start, basic_time_t * end) {
  basic_time_t diff;
  diff = basic_time_diff (start, end);
  return basic_time_float (&diff);
}

void basic_timer_init (basic_timer_t * timer, int fps) {
  memset (timer, 0, sizeof(basic_timer_t));
  timer->first_frame = basic_time_get ();
  timer->past_frame = timer->first_frame;
  timer->last_frame = timer->first_frame;

  if (fps != 0) {
    timer->target_fps = fps;
    timer->per_frame.nsec = NSEC / fps;
  }
}

void basic_timer_update (basic_timer_t * timer) {
  basic_time_t new_frame = basic_time_get ();
  timer->diff_frame = basic_time_diff (&timer->past_frame, &new_frame);
  timer->past_frame = new_frame;
  return ;
}

int basic_timer_frames (basic_timer_t * timer) {
  basic_timer_update (timer);
  basic_time_t * elasped = &timer->elasped, * per_frame = &timer->per_frame;
  elasped->sec += timer->diff_frame.sec;
  elasped->nsec += timer->diff_frame.nsec;
  while (elasped->nsec >= NSEC) {
    elasped->sec++;
    elasped->nsec -= NSEC;
  }
  int frames = 0;
  while (elasped->sec > per_frame->sec ||
  (elasped->sec == per_frame->sec && elasped->nsec >= per_frame->nsec)) {
    elasped->sec -= per_frame->sec;
    elasped->nsec -= per_frame->nsec;
    if (elasped->nsec < 0) {
      elasped->sec--;
      elasped->nsec += NSEC;
    }
    frames++;
    timer->dt = basic_time_fdiff (&timer->last_frame, &timer->past_frame);
    timer->last_frame = timer->past_frame;
  }
  return frames;
}

#ifdef BASIC_PROFILING
#include <sys/resource.h>

struct timeval timeval_diff(struct timeval start, struct timeval end) {
  struct timeval temp;
  if ((end.tv_usec-start.tv_usec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_usec = 1000000+end.tv_usec-start.tv_usec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_usec = end.tv_usec-start.tv_usec;
  }
  return temp;
}

void print_diff_rusage (struct rusage usage1, struct rusage usage2, const char * when) {
  struct timeval timediff;

  printf ("profiling: '%s'\n",when);
  timediff = timeval_diff (usage1.ru_utime, usage2.ru_utime);
  printf("user_time: %ld.%06ld seconds\n", timediff.tv_sec, timediff.tv_usec);
  timediff = timeval_diff (usage1.ru_stime, usage2.ru_stime);
  printf("sys_time: %ld.%06ld seconds\n", timediff.tv_sec, timediff.tv_usec);

  if (0) {
    printf(" /* integral shared memory size */      %8ld\n",  usage2.ru_ixrss - usage1.ru_ixrss );
    printf(" /* integral unshared data  */          %8ld\n",  usage2.ru_idrss - usage1.ru_idrss);
    printf(" /* integral unshared stack  */         %8ld\n",  usage2.ru_isrss - usage1.ru_isrss);
    printf(" /* page reclaims */                    %8ld\n",  usage2.ru_minflt - usage1.ru_minflt);
    printf(" /* page faults */                      %8ld\n",  usage2.ru_majflt - usage1.ru_majflt);
    printf(" /* swaps */                            %8ld\n",  usage2.ru_nswap  - usage1.ru_nswap);
    printf(" /* block input operations */           %8ld\n",  usage2.ru_inblock - usage1.ru_inblock);
    printf(" /* block output operations */          %8ld\n",  usage2.ru_oublock - usage1.ru_oublock);
    printf(" /* messages sent */                    %8ld\n",  usage2.ru_msgsnd - usage1.ru_msgsnd);
    printf(" /* messages received */                %8ld\n",  usage2.ru_msgrcv - usage1.ru_msgrcv);
    printf(" /* signals received */                 %8ld\n",  usage2.ru_nsignals - usage1.ru_nsignals);
    printf(" /* voluntary context switches */       %8ld\n",  usage2.ru_nvcsw - usage1.ru_nvcsw);
    printf(" /* involuntary  */                     %8ld\n",  usage2.ru_nivcsw - usage1.ru_nivcsw);
  }
}
#endif

