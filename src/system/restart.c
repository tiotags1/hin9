
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>

#include <hin.h>

void hin_stop () {
  master.quit = 1;
  void httpd_timer_flush ();
  httpd_timer_flush ();
}

int hin_restart () {
  printf("hin restart ...\n");
  int pid = fork ();
  if (pid != 0) {
    printf ("wait restart issued to former process\n");
    master.share->done = 0;
    master.wait_restart = 1;
    return 0;
  }
  int hin_event_clean ();
  hin_event_clean ();
  printf ("running exe file '%s'\n", master.exe_path);
  char * buf = NULL;
  asprintf (&buf, "--reuse=%d", master.sharefd);
  char * argv[] = {master.exe_path, buf, NULL};
  execvp (master.exe_path, argv);
  printf ("crash\n");
}

static void sig_restart (int signo) {
  hin_restart ();
}

static void sig_child (int signo) {
  //printf ("got sigchld\n");
}

int install_sighandler () {
  // SIGPIPE
  sigset_t set;
  sigemptyset (&set);
  sigaddset (&set, SIGPIPE);
  if (pthread_sigmask (SIG_BLOCK, &set, NULL) != 0)
    return -1;

  // SIGUSR1 -- restart
  struct sigaction new_action, old_action;

  // Set up the structure to specify the new action.
  new_action.sa_handler = sig_restart;
  sigemptyset (&new_action.sa_mask);
  new_action.sa_flags = 0;

  sigaction (SIGUSR1, NULL, &old_action);
  if (old_action.sa_handler != SIG_IGN)
    sigaction (SIGUSR1, &new_action, NULL);

  signal (SIGCHLD, sig_child);

  return 0;
}

static int hin_use_sharedmem (int sharefd) {
  hin_master_share_t * share = mmap (NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, sharefd, 0);
  master.share = share;
  /*printf ("resuing sockets\n");
  for (int i=0; i<share->nsocket; i++) {
    hin_master_socket_t * sock = &share->sockets[i];
    printf ("%d. socket %d\n", i, sock->sockfd);
  }*/
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


