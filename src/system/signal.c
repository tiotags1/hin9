
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <sys/wait.h>
#include <sys/signalfd.h>

#include "hin.h"

#define RESTART_SIGNAL SIGUSR1

static void hin_sig_restart_handler (int signo, siginfo_t * info, void * ucontext) {
  hin_restart ();
}

static void hin_sig_child_handler (int signo, siginfo_t * info, void * ucontext) {
  pid_t pid;
  int status;

  while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
    if (WIFSIGNALED (status)) {
      if (WTERMSIG (status) == SIGSEGV) {
        // It was terminated by a segfault
        printf ("child %d sigsegv\n", pid);
      } else {
        printf ("child %d terminated due to another signal\n", pid);
      }
    }
    #if HIN_HTTPD_WORKER_PREFORKED
    int hin_worker_closed (int pid);
    if (hin_worker_closed (pid) <= 0) {
      printf ("child %d died\n", pid);
    }
    #endif
    if (master.restart_pid == pid) {
      printf ("restart to %d failed status %d\n", pid, status);
      master.restart_pid = 0;
    } else {
      if (master.debug & DEBUG_CHILD)
        printf ("child %d terminated status %d\n", pid, status);
    }
  }
}

static void hin_sig_pipe_handler (int signo, siginfo_t * info, void * ucontext) {
  printf ("broken pipe signal received\n");
}

static void hin_sig_int_handler (int signo, siginfo_t * info, void * ucontext) {
  printf("^C pressed. Shutting down.\n");
  void hin_clean ();
  hin_clean ();
  exit (0);
}

#if (HIN_USE_SIGNAL_FD == 0)

int hin_signal_clean () {
  signal(SIGINT, SIG_DFL);
  signal(RESTART_SIGNAL, SIG_DFL);
  signal(SIGPIPE, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
  return 0;
}

int hin_signal_install () {
  sigset_t mask;
  sigemptyset (&mask);
  sigaddset (&mask, RESTART_SIGNAL);

  if (sigprocmask (SIG_UNBLOCK, &mask, NULL) < 0)
    perror ("sigprocmask");

  // It's better to use sigaction() over signal().  You won't run into the
  // issue where BSD signal() acts one way and Linux or SysV acts another.
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;

  sa.sa_sigaction = hin_sig_int_handler;
  sigaction (SIGINT, &sa, NULL);

  sa.sa_sigaction = hin_sig_pipe_handler;
  sigaction (SIGPIPE, &sa, NULL);

  sa.sa_sigaction = hin_sig_restart_handler;
  sigaction (RESTART_SIGNAL, &sa, NULL);

  sa.sa_sigaction = hin_sig_child_handler;
  sigaction (SIGCHLD, &sa, NULL);

  return 0;
}

#else

static sigset_t mask;
static hin_buffer_t * signal_buffer = NULL;

int hin_signal_clean () {
  if (sigprocmask (SIG_UNBLOCK, &mask, NULL) == -1)
    perror ("sigprocmask");
  if (signal_buffer) {
    close (signal_buffer->fd);
    hin_buffer_clean (signal_buffer);
    signal_buffer = NULL;
  }
  return 0;
}

static int hin_signal_callback1 (hin_buffer_t * buf, int ret) {
  struct signalfd_siginfo * info = (void*)buf->ptr;
  printf ("got sighandler\n");

  if (ret < 0) {
    printf ("signal error read '%s'\n", strerror (-ret));
    if (hin_request_read (buf) < 0) {
      printf ("signalfd failed\n");
      return -1;
    }
    return 0;
  }

  switch (info->ssi_signo) {
  case SIGINT:
    hin_sig_int_handler (info->ssi_signo, info, NULL);
  break;
  case RESTART_SIGNAL:
    hin_sig_restart_handler (info->ssi_signo, info, NULL);
  break;
  case SIGPIPE:
    hin_sig_pipe_handler (info->ssi_signo, info, NULL);
  break;
  case SIGCHLD:
    hin_sig_child_handler (info->ssi_signo, info, NULL);
  break;
  default:
    printf ("error got unexpected signal\n");
    return -1;
  break;
  }

  if (hin_request_read (buf) < 0) {
    printf ("signalfd failed\n");
    return -1;
  }

  return 0;
}

int hin_signal_install () {
  sigemptyset (&mask);
  sigaddset (&mask, SIGINT);
  sigaddset (&mask, RESTART_SIGNAL);
  sigaddset (&mask, SIGPIPE);
  sigaddset (&mask, SIGCHLD);

  if (sigprocmask (SIG_BLOCK, &mask, NULL) < 0)
    perror ("sigprocmask");

  int sig_fd = signalfd (-1, &mask, 0);
  if (sig_fd < 0)
    perror ("signalfd");

  hin_buffer_t * buf = malloc (sizeof (*buf) + sizeof (struct signalfd_siginfo));
  memset (buf, 0, sizeof (*buf));
  #ifdef HIN_LINUX_URING_DONT_HAVE_SIGNALFD
  buf->flags = HIN_EPOLL;
  #endif
  buf->fd = sig_fd;
  buf->callback = hin_signal_callback1;
  buf->count = buf->sz = sizeof (struct signalfd_siginfo);
  buf->ptr = buf->buffer;
  buf->debug = 0xffffffff;
  signal_buffer = buf;

  printf ("installed sighandler fd %d\n", sig_fd);

  if (hin_request_read (buf) < 0) {
    printf ("signalfd failed\n");
    return -1;
  }

  return 0;
}

#endif


