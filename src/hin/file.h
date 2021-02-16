#ifndef HIN_FILE_H
#define HIN_FILE_H

#include <stdint.h>
#include <time.h>

#include <basic_hashtable.h>

enum { HIN_CACHE_DONE = 0x1, HIN_CACHE_PUBLIC = 0x2, HIN_CACHE_IMMUTABLE = 0x4, HIN_CACHE_ERROR = 0x8 };

typedef struct {
  uint32_t flags;
  time_t max_age;
} hin_cache_data_t;

typedef struct hin_cache_client_struct {
  void * ptr;
  struct hin_cache_client_struct * next;
} hin_cache_client_queue_t;

typedef struct hin_cache_item_struct {
  int type;
  int refcount;
  uint32_t flags;

  int fd;
  basic_ht_hash_t cache_key1, cache_key2;
  time_t lifetime;

  time_t modified;
  off_t size;
  uint64_t etag;

  void * parent;
  hin_cache_client_queue_t * client_queue;
} hin_cache_item_t;


#endif

