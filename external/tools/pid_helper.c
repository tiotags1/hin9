
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEBUG_MSG 0
#define RESPOND_EVENT (IN_CLOSE_WRITE)

typedef struct {
  const char * read_path;
  const char * write_path;
} helper_t;

int inotify_fd = -1;

static int do_move (helper_t * helper) {
  int infd = open (helper->read_path, O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
  if (infd < 0) {
    fprintf (stderr, "can't open '%s': %s\n", helper->read_path, strerror (errno));
    return -1;
  }

  char buffer[128];
  int rd = read (infd, buffer, sizeof (buffer));
  if (rd < 0) {
    perror ("read");
    return -1;
  }
  buffer[rd] = '\0';

  int num = atoi (buffer);
  if (num <= 0) {
    fprintf (stderr, "can't find pid in '%.*s' %d\n", rd, buffer, num);
    return -1;
  }

  int outfd = open (helper->write_path, O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC, 0644);
  if (outfd < 0) {
    fprintf (stderr, "can't open '%s': %s\n", helper->write_path, strerror (errno));
    return -1;
  }

  int sz = snprintf (buffer, sizeof buffer, "%d\n", num);
  int wr = write (outfd, buffer, sz);
  if (wr < 0) {
    perror ("write");
    return -1;
  }
  if (close (outfd) < 0) {
    perror ("close");
    return -1;
  }
  exit (0);
  return 0;
}

static int do_readd (helper_t * helper) {
  int fd = open (helper->read_path, O_WRONLY|O_CREAT|O_NOFOLLOW|O_CLOEXEC, 0666);
  if (fd < 0) {
    fprintf (stderr, "can't open '%s': %s\n", helper->read_path, strerror (errno));
    return -1;
  }
  if (fchmod (fd, 0666) < 0) {
    perror ("fchmod");
    return -1;
  }
  if (close (fd) < 0) {
    perror ("close");
    return -1;
  }

  int watch = inotify_add_watch (inotify_fd, helper->read_path, RESPOND_EVENT);
  if (watch < 0) {
    fprintf (stderr, "vfs cannot watch '%s': %s\n", helper->read_path, strerror (errno));
    return -1;
  }
  return 0;
}

static int inotify_event (helper_t * helper, const struct inotify_event * event) {
  if (DEBUG_MSG) {
    if (event->mask & IN_CREATE) { printf ("IN_CREATE: "); }
    if (event->mask & IN_MODIFY) { printf ("IN_MODIFY: "); }
    if (event->mask & IN_MOVED_TO) { printf ("IN_MOVED_TO: "); }
    if (event->mask & IN_CLOSE_WRITE) { printf ("IN_CLOSE_WRITE: "); }
    if (event->mask & IN_CLOSE_NOWRITE) { printf ("IN_CLOSE_NOWRITE: "); }
    if (event->mask & IN_IGNORED) { printf ("IN_IGNORED: "); }
    printf ("event %x fn '%.*s'\n", event->mask, event->len, event->name);
  }
  if (event->mask & RESPOND_EVENT) {
    int ret = do_move (helper);
    if (ret < 0) return ret;
  }
  if (event->mask & IN_IGNORED) {
    int ret = do_readd (helper);
    if (ret < 0) return ret;
  }
  return 0;
}

static int inotify_events (helper_t * helper, char * buf, int len) {
  const struct inotify_event *event;

  if (len <= 0) return -1;

  for (char *ptr = buf; ptr < buf + len;
    ptr += sizeof(struct inotify_event) + event->len) {

    event = (const struct inotify_event *) ptr;
    int ret = inotify_event (helper, event);
    if (ret < 0) return ret;
  }

  return 0;
}

int main (int argc, const char * argv[]) {
  if (argc < 2) {
    printf ("program reads a number from a file and writes to another\n");
    printf ("usage: <prog> <read file> <write file>\n");
    return -1;
  }

  helper_t * helper = calloc (1, sizeof (*helper));
  helper->read_path = strdup (argv[1]);
  helper->write_path = strdup (argv[2]);

  if (inotify_fd <= 0) {
    inotify_fd = inotify_init1 (0); // IN_NONBLOCK
    if (inotify_fd < 0) {
      perror ("inotify_init1");
      return -1;
    }
  }

  if (do_readd (helper) < 0) {
    return -1;
  }

  if (daemon (0, 1) < 0) {
    perror ("daemon");
    return -1;
  }

  while (1) {
    char buf[1024];
    int rd = read (inotify_fd, buf, sizeof (buf));
    if (rd < 0) { perror ("inot_read"); return -1; }
    int ret = inotify_events (helper, buf, rd);
    if (ret < 0) return ret;
  }

  close (inotify_fd);

  return 0;
}


