
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

#include "hin.h"

int hin_check_alive () {
  if (master.restart_pid) {
    if (master.share->done == 0) {
      printf ("not done\n");
    } else if (master.share->done > 0) {
      printf ("restart succesful\n");
      hin_stop ();
      master.share->done = 0;
    } else {
      printf ("failed to restart\n");
      master.share->done = 0;
    }
  }
  if (master.quit == 0) return 1;
  if (master.debug & DEBUG_OTHER) printf ("hin live client %d conn %d\n", master.num_client, master.num_connection);
  if (master.num_client > 0) return 1;
  if (master.num_connection > 0) return 1;

  void hin_clean ();
  hin_clean ();

  exit (0);
}

void hin_stop () {
  master.quit = 1;
  void httpd_proxy_connection_close_all ();
  httpd_proxy_connection_close_all ();
  void httpd_timer_flush ();
  httpd_timer_flush ();
  hin_check_alive ();
}

int hin_restart () {
  printf("hin restart ...\n");
  int pid = fork ();
  if (pid != 0) {
    printf ("wait restart issued to former process\n");
    master.share->done = 0;
    master.restart_pid = pid;
    return 0;
  }
  int hin_event_clean ();
  hin_event_clean ();
  printf ("running exe file '%s'\n", master.exe_path);
  char * buf = NULL;
  if (asprintf (&buf, "--reuse=%d", master.sharefd) < 0)
    perror ("asprintf");
  char * argv[] = {master.exe_path, buf, NULL};
  execvp (master.exe_path, argv);
  printf ("crash\n");
}

static void sig_restart (int signo) {
  hin_restart ();
}

static void sig_child_handler (int signo) {
  pid_t pid;
  int status;

  while ((pid = waitpid (-1, &status, WNOHANG)) > 0) {
    if (WIFSIGNALED (status)) {
      if (WTERMSIG(status) == SIGSEGV) {
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

int install_sighandler () {
  // It's better to use sigaction() over signal().  You won't run into the
  // issue where BSD signal() acts one way and Linux or SysV acts another.
  struct sigaction sa;
  memset (&sa, 0, sizeof sa);
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;

  sa.sa_handler = SIG_IGN;
  sigaction (SIGPIPE, &sa, NULL);

  sa.sa_handler = sig_restart;
  sigaction (SIGUSR1, &sa, NULL);

  sa.sa_handler = sig_child_handler;
  sigaction (SIGCHLD, &sa, NULL);

  return 0;
}

static int hin_use_sharedmem (int sharefd) {
  hin_master_share_t * share = mmap (NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, sharefd, 0);
  master.share = share;
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
}

int hin_init_sharedmem () {
  if (master.sharefd == 0) {
    hin_create_sharedmem ();
  } else {
    hin_use_sharedmem (master.sharefd);
  }
}

