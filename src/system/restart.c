
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signalfd.h>

#include "hin.h"

int hin_check_alive () {
  if (master.restarting) {
    if (master.share->done == 0) {
      printf ("not done\n");
    } else if (master.share->done > 0) {
      printf ("restart succesful\n");
      master.share->done = 0;
      master.restarting = 0;
      hin_stop ();
    } else {
      printf ("failed to restart\n");
      master.share->done = 0;
    }
  }
  if (master.quit == 0) return 1;
  if (master.debug & DEBUG_CONFIG) printf ("hin live client %d conn %d\n", master.num_client, master.num_connection);
  if (master.num_client > 0) return 1;
  if (master.num_connection > 0) return 1;

  void hin_clean ();
  hin_clean ();

  exit (0);
  return -1;
}

int hin_check_alive_timer () {
  if (master.restart_pid && master.share->done) {
    hin_stop ();
    master.share->done = 0;
  }
  return 0;
}

void hin_stop () {
  master.quit = 1;
  void httpd_proxy_connection_close_all ();
  httpd_proxy_connection_close_all ();
  void httpd_timer_flush ();
  httpd_timer_flush ();
  hin_check_alive ();
}

static void hin_restart_old () {
  master.restarting = 1;
}

static void hin_restart_new () {
  int hin_event_clean ();
  hin_event_clean ();
  printf ("restart exe file '%s'\n", master.exe_path);
  char * buf = NULL;
  if (asprintf (&buf, "%d", master.sharefd) < 0)
    perror ("asprintf");
  char * argv[] = {master.exe_path, "--reuse", buf, NULL};
  execvp (master.exe_path, argv);
  perror ("execvp");
  exit (-1);
}

int hin_restart () {
  printf("hin restart ...\n");

  master.share->done = 0;

  int pid = fork ();
  if (pid == 0) {
    hin_restart_new ();
    return 0;
  }

  printf ("restart %d into %d\n", getpid (), pid);
  master.restart_pid = pid;
  hin_restart_old ();

  return 0;
}

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
  signal(SIGUSR1, SIG_DFL);
  signal(SIGPIPE, SIG_DFL);
  signal(SIGCHLD, SIG_DFL);
  return 0;
}

int hin_signal_install () {
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
  sigaction (SIGUSR1, &sa, NULL);

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
    hin_request_read (buf); // ?
    return 0;
  }

  switch (info->ssi_signo) {
  case SIGINT:
    hin_sig_int_handler (info->ssi_signo, info, NULL);
  break;
  case SIGUSR1:
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

  hin_request_read (buf);

  return 0;
}

int hin_signal_install () {
  sigemptyset (&mask);
  sigaddset (&mask, SIGINT);
  sigaddset (&mask, SIGUSR1);
  sigaddset (&mask, SIGPIPE);
  sigaddset (&mask, SIGCHLD);

  if (sigprocmask (SIG_BLOCK, &mask, NULL) < 0)
    perror ("sigprocmask");

  int sig_fd = signalfd (-1, &mask, 0);
  if (sig_fd < 0)
    perror ("signalfd");

  hin_buffer_t * buf = malloc (sizeof (*buf) + sizeof (struct signalfd_siginfo));
  memset (buf, 0, sizeof (*buf));
  buf->flags = 0;
  buf->fd = sig_fd;
  buf->callback = hin_signal_callback1;
  buf->count = buf->sz = sizeof (struct signalfd_siginfo);
  buf->ptr = buf->buffer;
  buf->debug = 0xffffffff;
  signal_buffer = buf;

  printf ("installed sighandler fd %d\n", sig_fd);

  hin_request_read (buf);

  return 0;
}

#endif

static int hin_use_sharedmem (int sharefd) {
  hin_master_share_t * share = mmap (NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, sharefd, 0);
  master.share = share;
  return 0;
}

static int hin_create_sharedmem () {
  int sharefd = memfd_create ("secret_file?", 0);
  if (sharefd < -1) {
    perror ("memfd_create");
    return -1;
  }
  if (ftruncate (sharefd, 4096) < -1) {
    perror ("truncate");
    return -1;
  }
  hin_master_share_t * share = mmap (NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, sharefd, 0);
  master.share = share;
  master.sharefd = sharefd;
  memset (share, 0, 4096);
  return 0;
}

int hin_init_sharedmem () {
  if (master.sharefd == 0) {
    return hin_create_sharedmem ();
  } else {
    return hin_use_sharedmem (master.sharefd);
  }
}

void hin_sharedmem_clean () {
  if (munmap (master.share, 4096) < 0) perror ("munmap");
  if (close (master.sharefd) < 0) perror ("share close");
}

