#ifndef HIN_FILE_H
#define HIN_FILE_H

#include <stdint.h>
#include <time.h>

#include <basic_hashtable.h>

typedef struct hin_cache_client_struct {
  void * ptr;
  struct hin_cache_client_struct * next;
} hin_cache_client_queue_t;

typedef struct hin_cache_item_struct {
  int type;
  int refcount;

  int fd;
  basic_ht_hash_t cache_key1, cache_key2;
  time_t life;

  time_t modified;
  off_t size;
  uint64_t etag;

  hin_cache_client_queue_t * client_queue;
} hin_cache_item_t;


#endif

