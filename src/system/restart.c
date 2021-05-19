
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "hin.h"
#include "worker.h"

int hin_check_alive () {
  if (master.flags & HIN_RESTARTING) {
    if (master.share->done == 0) {
      printf ("not done\n");
    } else if (master.share->done > 0) {
      printf ("restart succesful\n");
      master.share->done = 0;
      master.flags &= (~HIN_RESTARTING);
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

static void hin_restart_do_close () {
  master.flags |= HIN_RESTARTING;
  hin_stop ();
}

static void hin_restart_do_exec () {
  //int hin_event_clean ();
  //hin_event_clean ();
  printf ("restart to '%s'\n", master.exe_path);
  char * buf = NULL;
  if (asprintf (&buf, "%d", master.sharefd) < 0)
    perror ("asprintf");
  const char ** argv = calloc (20, sizeof (char*));
  int i=0;
  argv[i++] = master.exe_path;
  argv[i++] = "--reuse";
  argv[i++] = buf;
  argv[i++] = "--config";
  argv[i++] = master.conf_path;
  argv[i++] = "--workdir_path";
  argv[i++] = master.workdir_path;
  argv[i++] = "--logdir";
  argv[i++] = master.logdir_path;
  if (master.pid_path) {
    argv[i++] = "--pidfile";
    argv[i++] = master.pid_path;
  }
  argv[i++] = NULL;
  execvp (master.exe_path, (char * const*)argv);
  printf ("execvp '%s' error: %s\n", master.exe_path, strerror (errno));
  exit (-1);
}

static int hin_restart_child_done (hin_child_t * child, int ret) {
  printf ("restart child done\n");
  master.share->done = 1;
  return 0;
}

int hin_restart1 () {
  printf("hin restart ...\n");

  master.share->done = 0;

  int hin_pidfile_clean ();
  hin_pidfile_clean ();

  int pid = fork ();
  if (pid < 0) {
    perror ("fork");
    return -1;
  }
  if (pid == 0) {
    hin_restart_do_exec ();
    return 0;
  }

  printf ("restarting %d helper %d\n", getpid (), pid);
  master.restart_pid = pid;

  hin_child_t * child = calloc (1, sizeof (hin_child_t));
  child->pid = pid;
  child->callback = hin_restart_child_done;
  hin_children_add (child);

  hin_restart_do_close ();

  return 0;
}

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

