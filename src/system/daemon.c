
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>

#include "hin.h"
#include "system/system.h"

int hin_pidfile (const char * path) {
  int fd = openat (AT_FDCWD, path, O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, 0644);
  if (fd < 0) {
    if (errno == EEXIST) {
      printf ("should clean up old pidfile %s\n", path);
    }
    printf ("can't open '%s' %s\n", path, strerror (errno));
    exit (1);
  }
  char buf[256];
  int ret = snprintf (buf, sizeof buf, "%d\n", getpid ());
  if (ret < 0) return -1;
  int err = write (fd, buf, ret);
  if (err < ret) { perror ("write"); return -1; }
  err = close (fd);
  if (err < 0) { perror ("close"); return -1; }
  return 0;
}

int hin_pidfile_clean () {
  if (master.pid_path) {
    if (unlinkat (AT_FDCWD, master.pid_path, 0) < 0)
      perror ("unlinkat");
  }
  return 0;
}

int hin_daemonize () {
  pid_t pid;

  pid = fork();
  if (pid < 0) {
    perror ("fork");
    exit (EXIT_FAILURE);
  }
  if (pid > 0)
    exit (EXIT_SUCCESS);

  if (setsid() < 0) {
    perror ("setsid");
    exit (EXIT_FAILURE);
  }

  //TODO: Implement a working signal handler
  signal (SIGCHLD, SIG_IGN);
  signal (SIGHUP, SIG_IGN);

  pid = fork();

  if (pid < 0) {
    perror ("fork");
    exit (EXIT_FAILURE);
  }

  if (pid > 0)
    exit (EXIT_SUCCESS);

  umask (0);

  // Change the working directory to the root directory
  // or another appropriated directory
  //chdir ("/");

  for (int x = sysconf (_SC_OPEN_MAX); x >= 3; x--) {
    //close (x);
  }

  // Open the log file
  openlog ("hin", LOG_PID, LOG_DAEMON);
  return 0;
}


int hin_redirect_log (const char * path) {
  int fd = hin_open_file_and_create_path (AT_FDCWD, path, O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT, 0660);
  if (fd < 0) {
    printf ("hin can't open log '%s' %s\n", path, strerror (errno));
    exit (1);
  }

  fflush (stdout);
  fflush (stderr);
  if (dup2 (fd, STDOUT_FILENO) < 0) perror ("dup2 stdout");
  if (dup2 (fd, STDERR_FILENO) < 0) perror ("dup2 stderr");
  close (fd);

  if (master.debug & DEBUG_CONFIG)
    printf ("create log on %d '%s'\n", fd, path);

  return 0;
}

#include <sys/resource.h>
#include "conf.h"

int hin_linux_set_limits () {
  struct rlimit new;
  rlim_t max;

  memset (&new, 0, sizeof (new));
  if (getrlimit (RLIMIT_MEMLOCK, &new) < 0) {
    perror ("getrlimit");
  }

  max = new.rlim_max;
  max = max > HIN_RLIMIT_MEMLOCK ? HIN_RLIMIT_MEMLOCK : max;
  if (master.debug & DEBUG_INFO)
    printf ("rlimit MEMLOCK %lld/%lld/%lld\n", (long long)new.rlim_cur, (long long)max, (long long)new.rlim_max);

  new.rlim_cur = max;
  if (setrlimit (RLIMIT_MEMLOCK, &new) < 0) {
    perror ("setrlimit");
  }

  if (new.rlim_cur < HIN_RLIMIT_MEMLOCK) {
    fprintf (stderr, "WARNING! low RLIMIT_MEMLOCK, possible crashes, message possibly outdated\n");
    fprintf (stderr, " current: %lld\n", (long long)new.rlim_cur);
    fprintf (stderr, " suggested: %lld\n", (long long)HIN_RLIMIT_MEMLOCK);
  }

  memset (&new, 0, sizeof (new));
  if (getrlimit (RLIMIT_NOFILE, &new) < 0) {
    perror ("getrlimit");
  }

  max = new.rlim_max;
  max = max > HIN_RLIMIT_NOFILE ? HIN_RLIMIT_NOFILE : max;
  if (master.debug & DEBUG_INFO)
    printf ("rlimit NOFILE %lld/%lld/%lld\n", (long long)new.rlim_cur, (long long)max, (long long)new.rlim_max);

  new.rlim_cur = max;
  if (setrlimit (RLIMIT_NOFILE, &new) < 0) {
    perror ("setrlimit");
  }
  return 0;
}

