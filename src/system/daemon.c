
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

int hin_pidfile (const char * path) {
  int fd = openat (AT_FDCWD, path, O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC | O_EXCL, S_IRWXU);
  if (fd < 0) {
    if (errno == EEXIST) {
      printf ("should clean up old pidfile %s\n", path);
    }
    printf ("can't open '%s' %s\n", path, strerror (errno));
    return 0;
  }
  char buf[256];
  int ret = snprintf (buf, sizeof buf, "%d", getpid ());
  if (ret < 0) return -1;
  int err = write (fd, buf, ret);
  if (err < ret) { perror ("write"); return -1; }
  err = close (fd);
  if (err < 0) { perror ("close"); return -1; }
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
    exit(EXIT_SUCCESS);

  if (setsid() < 0) {
    perror ("setsid");
    exit (EXIT_FAILURE);
  }

  //TODO: Implement a working signal handler */
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

  /* Change the working directory to the root directory */
  /* or another appropriated directory */
  //chdir ("/");

  for (int x = sysconf (_SC_OPEN_MAX); x >= 3; x--) {
    //close (x);
  }

  /* Open the log file */
  openlog ("hin", LOG_PID, LOG_DAEMON);
  return 0;
}


