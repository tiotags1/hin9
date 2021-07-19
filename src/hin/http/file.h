#ifndef HIN_FILE_H
#define HIN_FILE_H

#include <stdint.h>
#include <time.h>

#include <basic_hashtable.h>

enum { HIN_CACHE_DONE = 0x1, HIN_CACHE_ERROR = 0x2, HIN_CACHE_PUBLIC = 0x4, HIN_CACHE_PRIVATE = 0x8,
 HIN_CACHE_NO_CACHE = 0x10, HIN_CACHE_NO_STORE = 0x20, HIN_CACHE_NO_TRANSFORM = 0x40, HIN_CACHE_IMMUTABLE = 0x80,
 HIN_CACHE_MUST_REVALIDATE = 0x100, HIN_CACHE_PROXY_REVALIDATE = 0x200, HIN_CACHE_MAX_AGE = 0x400 };

typedef struct hin_cache_client_struct {
  void * ptr;
  struct hin_cache_client_struct * next;
} hin_cache_client_queue_t;

typedef struct hin_cache_item_struct {
  int type;
  int fd;
  uint32_t flags;
  uint32_t magic;
  void * parent;

  int refcount;

  basic_ht_hash_t cache_key1, cache_key2;
  hin_timer_t timer;

  time_t modified;
  off_t size;
  uint64_t etag;

  hin_cache_client_queue_t * client_queue;
} hin_cache_item_t;


#endif

