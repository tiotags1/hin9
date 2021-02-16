
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
  basic_ht_t ht;
} hin_cache_store_t;

static hin_cache_store_t * default_store = NULL;

hin_cache_store_t * hin_cache_create () {
  hin_cache_store_t * store = calloc (1, sizeof (hin_cache_store_t));
  if (basic_ht_init (&store->ht, 1024) < 0) {
    printf ("error in hashtable init\n");
  }
  if (default_store == NULL)
    default_store = store;

  return store;
}

void hin_cache_item_clean (hin_cache_item_t * item) {
  if (item->fd > 0) close (item->fd);
  if (master.debug & DEBUG_CACHE) printf ("cache item %lx_%lx freed\n", item->cache_key1, item->cache_key2);

  if (HIN_HTTPD_CACHE_CLEAN_ON_EXIT) {
    char buffer[sizeof (HIN_HTTPD_CACHE_DIRECTORY) + 70];
    snprintf (buffer, sizeof buffer, HIN_HTTPD_CACHE_DIRECTORY "/%lx_%lx", item->cache_key1, item->cache_key2);
    if (unlinkat (AT_FDCWD, buffer, 0) < 0) perror ("unlinkat");
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
    if (master.debug & DEBUG_CACHE) printf ("cache free %lx_%lx\n", item->cache_key1, item->cache_key2);
    hin_cache_item_clean (item);
  } else if (item->refcount < 0) {
    printf ("cache error refcount < 0\n");
  } else {
    if (master.debug & DEBUG_CACHE) printf ("cache refcount --%d %lx_%lx\n", item->refcount, item->cache_key1, item->cache_key2);
  }
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
  free (store);
}

void hin_cache_clean () {
  if (master.debug & DEBUG_CACHE) printf ("cache clean\n");
  hin_cache_store_clean (default_store);
}

hin_cache_item_t * hin_cache_get (hin_cache_store_t * store, basic_ht_hash_t key1, basic_ht_hash_t key2) {
  basic_ht_pair_t * pair = basic_ht_get_pair (&store->ht, key1, key2);
  if (pair == NULL) return NULL;
  hin_cache_item_t * item = (void*)pair->value1;
  return item;
}

int hin_cache_save (hin_cache_store_t * store, hin_pipe_t * pipe) {
  if (store == NULL) store = default_store;
  httpd_client_t * http = pipe->parent;

  if (http->disable & HIN_HTTP_LOCAL_CACHE) return -1;
  if ((http->cache_key1 | http->cache_key2) == 0) return -1;

  hin_cache_item_t * item = hin_cache_get (store, http->cache_key1, http->cache_key2);
  hin_cache_client_queue_t * queue = calloc (1, sizeof (*queue));
  queue->ptr = http;

  if (item) {
    // if cache object is present but incomplete register for completion watching and return
    queue->next = item->client_queue;
    item->client_queue = queue;
    item->refcount++;
    if (master.debug & DEBUG_CACHE) printf ("cache %lx_%lx queue event\n", http->cache_key1, http->cache_key2);
    return 0;
  } else {
    item = calloc (1, sizeof (*item));
    item->type = HIN_CACHE_OBJECT;
    item->refcount = 2;
    item->cache_key1 = http->cache_key1;
    item->cache_key2 = http->cache_key2;
    item->life = http->cache;
    basic_ht_set_pair (&store->ht, http->cache_key1, http->cache_key2, (uintptr_t)item, 0);
    item->client_queue = queue;
  }

  if (HIN_HTTPD_CACHE_TMPFILE) {
    item->fd = openat (AT_FDCWD, HIN_HTTPD_CACHE_DIRECTORY, O_RDWR | O_TMPFILE, 0600);
    if (item->fd < 0) perror ("openat");
  } else {
    char buffer[sizeof (HIN_HTTPD_CACHE_DIRECTORY) + 70];
    snprintf (buffer, sizeof buffer, HIN_HTTPD_CACHE_DIRECTORY "/%lx_%lx", http->cache_key1, http->cache_key2);
    item->fd = openat (AT_FDCWD, buffer, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (master.debug & DEBUG_CACHE) printf ("cache %lx_%lx create fd %d path '%s'\n", http->cache_key1, http->cache_key2, item->fd, buffer);
    if (item->fd < 0) perror ("openat");
  }

  pipe->out.fd = item->fd;
  pipe->out.flags = (pipe->out.flags & ~(HIN_SOCKET|HIN_SSL)) | (HIN_FILE|HIN_OFFSETS);
  pipe->parent = item;

  // save php headers
  // how do you resend the static resource and notify cache object is done and should be sent
  return 1;
}

int httpd_send_file (httpd_client_t * http, hin_cache_item_t * item, hin_buffer_t * buf);

int hin_cache_finish (httpd_client_t * client, hin_pipe_t * pipe) {
  hin_cache_item_t * item = (hin_cache_item_t*)client;

  item->size = pipe->count;
  item->etag = 0;
  time (&item->modified);
  if (master.debug & DEBUG_CACHE) printf ("cache %lx_%lx finish sz %ld etag %lx\n", item->cache_key1, item->cache_key2, item->size, item->etag);

  hin_cache_client_queue_t * next;
  for (hin_cache_client_queue_t * queue = item->client_queue; queue; queue = next) {
    next = queue->next;

    httpd_client_t * http = queue->ptr;
    http->peer_flags &= ~(HIN_HTTP_CHUNKED);
    http->state &= ~HIN_REQ_DATA;
    httpd_send_file (http, item, NULL);
    free (queue);
  }
  item->client_queue = NULL;

  return 0;
}

int hin_cache_check (hin_cache_store_t * store, httpd_client_t * http) {
  if (store == NULL) store = default_store;

  if (http->disable & HIN_HTTP_LOCAL_CACHE) return -1;
  if ((http->cache_key1 | http->cache_key2) == 0) return -1;

  hin_cache_item_t * item = hin_cache_get (store, http->cache_key1, http->cache_key2);
  if (item == NULL) return 0;

  if (master.debug & DEBUG_CACHE) printf ("cache %lx_%lx present\n", http->cache_key1, http->cache_key2);

  item->refcount++;

  http->peer_flags &= ~(HIN_HTTP_CHUNKED);
  http->state &= ~HIN_REQ_DATA;
  httpd_send_file (http, item, NULL);

  return 1;
}

