
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>

#include "hin.h"
#include "http.h"
#include "file.h"
#include "conf.h"

#include <basic_hashtable.h>

typedef struct {
  int dirfd;
  off_t size, max_size;
  basic_ht_t ht;
} hin_cache_store_t;

static hin_cache_store_t * default_store = NULL;

uintptr_t hin_cache_seed () {
  return default_store->ht.seed;
}

// from https://github.com/robertdavidgraham/sockdoc/blob/e1a6fe43389f1033c126e52e8015e30bd26bed6b/src/util-entropy.c
static int hin_random (uint8_t * buf, int sz) {
  int fd;

  do {
    fd = open("/dev/urandom", O_RDONLY, 0);
  } while (fd < 0 && errno == EINTR);

  if (fd < 0) {
    do {
      fd = open("/dev/random", O_RDONLY, 0);
    } while (fd < 0 && errno == EINTR);
  }

  if (fd < 0) {
    printf ("random couldn't open /dev/urandom\n");
    return -1;
  }

  /* Read this a byte at a time. This is because in theory,
   * a single read of 64 bytes may result only in a smaller
   * chunk of data. This makes testing when this rarely
   * occurs difficult, so instead just force the case of
   * a byte-at-a-time */
  for (int i = 0; i < sz; i++) {
    int x = read (fd, buf + i, 1);
    if (x != 1) {
      printf ("random couldn't read /dev/urandom '%s'\n", strerror (errno));
      return -1;
    }
  }

  return 0;
}

hin_cache_store_t * hin_cache_create () {
  hin_cache_store_t * store = calloc (1, sizeof (hin_cache_store_t));
  uintptr_t seed;
  hin_random ((uint8_t*)&seed, sizeof (seed));
  if (basic_ht_init (&store->ht, 1024, seed) < 0) {
    printf ("error! %d\n", 1134557);
  }
  if (default_store == NULL)
    default_store = store;
  store->max_size = HIN_HTTPD_CACHE_MAX_SIZE;

  int slen = strlen (master.tmpdir_path) + sizeof ("/"HIN_HTTPD_CACHE_DIRECTORY"/db") + 1;
  char * new_path = malloc (slen);
  snprintf (new_path, slen, "%s"HIN_HTTPD_CACHE_DIRECTORY"/db", master.tmpdir_path);
  int hin_open_file_and_create_path (int dirfd, const char * path, int flags, mode_t mode);
  int fd = hin_open_file_and_create_path (AT_FDCWD, new_path, O_WRONLY | O_APPEND | O_CLOEXEC | O_CREAT, 0660);
  if (fd < 0) {
    fprintf (stderr, "openat: %s %s\n", strerror (errno), new_path);
  }
  close (fd);
  unlinkat (AT_FDCWD, new_path, 0);

  snprintf (new_path, slen, "%s"HIN_HTTPD_CACHE_DIRECTORY"/", master.tmpdir_path);
  store->dirfd = openat (AT_FDCWD, new_path, O_DIRECTORY, 0);
  if (store->dirfd < 0) {
    fprintf (stderr, "openat: %s %s\n", strerror (errno), new_path);
  }
  if (master.debug & DEBUG_CONFIG)
    printf ("cache '%s' seed %zx dirfd %d\n", new_path, seed, store->dirfd);

  free (new_path);

  return store;
}

void hin_cache_item_clean (hin_cache_item_t * item) {
  if (item->fd > 0) close (item->fd);
  if (master.debug & DEBUG_CACHE) printf ("cache %zx_%zx free\n", item->cache_key1, item->cache_key2);

  hin_timer_remove (&item->timer);

  if (HIN_HTTPD_CACHE_CLEAN_ON_EXIT) {
    hin_cache_store_t * store = item->parent;
    char buffer[70];
    snprintf (buffer, sizeof buffer, "%zx_%zx", item->cache_key1, item->cache_key2);
    if (unlinkat (store->dirfd, buffer, 0) < 0) perror ("unlinkat");
  }

  hin_cache_client_queue_t * next;
  for (hin_cache_client_queue_t * queue = item->client_queue; queue; queue = next) {
    next = queue->next;
    // TODO flush clients waiting for cache
    free (queue);
  }
  free (item);
}

void hin_cache_unref (hin_cache_item_t * item) {
  item->refcount--;
  if (item->refcount == 0) {
    hin_cache_item_clean (item);
  } else if (item->refcount < 0) {
    printf ("cache error refcount < 0\n");
  } else {
    if (master.debug & DEBUG_CACHE) printf ("cache %zx_%zx refcount --%d\n", item->cache_key1, item->cache_key2, item->refcount);
  }
}

void hin_cache_remove (hin_cache_store_t * store, hin_cache_item_t * item) {
  basic_ht_delete_pair (&store->ht, item->cache_key1, item->cache_key2);
  store->size -= item->size;
  hin_cache_unref (item);
}

void hin_cache_store_clean (hin_cache_store_t * store) {
  basic_ht_iterator_t iter;
  basic_ht_pair_t * pair;
  memset (&iter, 0, sizeof iter);
  while ((pair = basic_ht_iterate_pair (&store->ht, &iter)) != NULL) {
    hin_cache_item_t * item = (void*)pair->value1;
    hin_cache_unref (item);
  }
  basic_ht_clean (&store->ht);
  close (store->dirfd);
  free (store);
}

void hin_cache_clean () {
  if (master.debug & DEBUG_CACHE) printf ("cache clean\n");
  hin_cache_store_clean (default_store);
  default_store = NULL;
}

hin_cache_item_t * hin_cache_get (hin_cache_store_t * store, basic_ht_hash_t key1, basic_ht_hash_t key2) {
  basic_ht_pair_t * pair = basic_ht_get_pair (&store->ht, key1, key2);
  if (pair == NULL) return NULL;
  hin_cache_item_t * item = (void*)pair->value1;
  return item;
}

static int hin_cache_pipe_error_callback (hin_pipe_t * pipe, int err) {
  hin_cache_item_t * item = (hin_cache_item_t*)pipe->parent;
  hin_cache_store_t * store = item->parent;

  printf ("cache %zx_%zx error in generating\n", item->cache_key1, item->cache_key2);

  hin_cache_client_queue_t * next;
  for (hin_cache_client_queue_t * queue = item->client_queue; queue; queue = next) {
    next = queue->next;

    httpd_client_t * http = queue->ptr;
    if (master.debug & DEBUG_CACHE) printf (" error to %d\n", http->c.sockfd);
    http->state &= ~HIN_REQ_DATA;
    httpd_error (http, 500, "error! %d", 325346);
    hin_cache_unref (item);
    free (queue);
  }
  item->client_queue = NULL;
  item->flags |= HIN_CACHE_ERROR;

  basic_ht_delete_pair (&store->ht, item->cache_key1, item->cache_key2);

  return 0;
}

int hin_cache_timeout_callback (hin_timer_t * timer, time_t time) {
  hin_cache_item_t * item = timer->ptr;
  hin_cache_store_t * store = item->parent;
  if (store == NULL) store = default_store;

  if (master.debug & (DEBUG_CACHE|DEBUG_TIMEOUT))
    printf ("cache %zx_%zx timeout for %p\n", item->cache_key1, item->cache_key2, item);
  hin_cache_remove (store, item);
  return 1;
}

int hin_cache_save (void * store1, hin_pipe_t * pipe) {
  hin_cache_store_t * store = store1;
  if (store == NULL) store = default_store;
  httpd_client_t * http = pipe->parent;

  if (http->disable & HIN_HTTP_LOCAL_CACHE) return -1;
  if ((http->cache_key1 | http->cache_key2) == 0) return -1;
  if (store->size >= store->max_size) {
    printf ("cache is full\n");
    return -1;
  }

  hin_cache_item_t * item = hin_cache_get (store, http->cache_key1, http->cache_key2);
  hin_cache_client_queue_t * queue = calloc (1, sizeof (*queue));
  queue->ptr = http;

  if (item) {
    // if cache object is present but incomplete register for completion watching and return
    queue->next = item->client_queue;
    item->client_queue = queue;
    item->refcount++;
    if (master.debug & DEBUG_CACHE) printf ("cache %zx_%zx queue event\n", http->cache_key1, http->cache_key2);
    return 0;
  }

  item = calloc (1, sizeof (*item));
  item->type = HIN_CACHE_OBJECT;
  item->refcount = 2;
  item->cache_key1 = http->cache_key1;
  item->cache_key2 = http->cache_key2;
  item->parent = store;
  item->client_queue = queue;
  basic_ht_set_pair (&store->ht, http->cache_key1, http->cache_key2, (uintptr_t)item, 0);

  if (HIN_HTTPD_CACHE_TMPFILE) {
    item->fd = openat (store->dirfd, ".", O_RDWR | O_TMPFILE, 0600);
    if (item->fd < 0) perror ("openat");
  } else {
    char buffer[70];
    snprintf (buffer, sizeof buffer, "%zx_%zx", http->cache_key1, http->cache_key2);
    item->fd = openat (store->dirfd, buffer, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (master.debug & DEBUG_CACHE) printf ("cache %zx_%zx create fd %d\n", http->cache_key1, http->cache_key2, item->fd);
    if (item->fd < 0) {
      perror ("openat");
      return -1;
    }
  }

  hin_timer_t * timer = &item->timer;
  timer->callback = hin_cache_timeout_callback;
  timer->ptr = item;
  hin_timer_update (timer, time (NULL) + http->cache);

  if (master.debug & (DEBUG_TIMEOUT|DEBUG_CACHE))
    printf ("cache %zx_%zx timeout %p at %lld\n", http->cache_key1, http->cache_key2, timer->ptr, (long long)timer->time);

  pipe->flags |= HIN_HASH;
  pipe->out.fd = item->fd;
  pipe->out.flags = (pipe->out.flags & ~(HIN_SOCKET|HIN_SSL)) | (HIN_FILE|HIN_OFFSETS);
  pipe->parent = item;
  pipe->out_error_callback = hin_cache_pipe_error_callback;
  pipe->in_error_callback = hin_cache_pipe_error_callback;

  // save php headers
  // how do you resend the static resource and notify cache object is done and should be sent
  return 1;
}

int httpd_send_file (httpd_client_t * http, hin_cache_item_t * item, hin_buffer_t * buf);

void hin_cache_serve_client (httpd_client_t * http, hin_cache_item_t * item) {
  http->peer_flags &= ~(HIN_HTTP_CHUNKED);
  http->status = 200;
  hin_timer_t * timer = &item->timer;
  void hin_cache_set_number (httpd_client_t * http, time_t num);
  hin_cache_set_number (http, timer->time - time (NULL));
  httpd_send_file (http, item, NULL);
}

int hin_cache_finish (httpd_client_t * client, hin_pipe_t * pipe) {
  hin_cache_item_t * item = (hin_cache_item_t*)client;
  if (item->flags & HIN_CACHE_ERROR) {
    hin_cache_unref (item);
    return 0;
  }

  item->size = pipe->count;
  item->etag = pipe->hash;
  item->flags |= HIN_CACHE_DONE;
  time (&item->modified);
  if (master.debug & DEBUG_CACHE) printf ("cache %zx_%zx ready sz %lld etag %llx\n", item->cache_key1, item->cache_key2, (long long)item->size, (long long)item->etag);

  hin_cache_store_t * store = item->parent;
  store->size += item->size;

  hin_cache_client_queue_t * next;
  for (hin_cache_client_queue_t * queue = item->client_queue; queue; queue = next) {
    next = queue->next;

    httpd_client_t * http = queue->ptr;
    hin_cache_serve_client (http, item);
    free (queue);
  }
  item->client_queue = NULL;

  return 0;
}

int hin_cache_check (void * store1, httpd_client_t * http) {
  hin_cache_store_t * store = store1;
  if (store == NULL) store = default_store;

  if (http->disable & HIN_HTTP_LOCAL_CACHE) return -1;
  if ((http->cache_key1 | http->cache_key2) == 0) return -1;

  hin_cache_item_t * item = hin_cache_get (store, http->cache_key1, http->cache_key2);
  if (item == NULL) return 0;

  if (master.debug & DEBUG_CACHE) printf ("cache %zx_%zx present\n", http->cache_key1, http->cache_key2);

  item->refcount++;

  hin_cache_serve_client (http, item);

  return 1;
}


