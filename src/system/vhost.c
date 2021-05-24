
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basic_hashtable.h>

#include "hin.h"

static basic_ht_t * vhost_ht = NULL;

int hin_vhost_add (const char * name, int name_len, void * ptr) {
  if (vhost_ht == NULL) {
    vhost_ht = basic_ht_create (1024, 101);
  }

  basic_ht_hash_t h1 = 0, h2 = 0;
  basic_ht_hash (name, name_len, vhost_ht->seed, &h1, &h2);
  basic_ht_set_pair (vhost_ht, h1, h2, 0, (uintptr_t)ptr);

  return 0;
}

void hin_vhost_clean () {
  if (vhost_ht)
    basic_ht_free (vhost_ht);
  vhost_ht = NULL;
}

void * hin_vhost_get (const char * name, int name_len) {
  if (vhost_ht == NULL) return NULL;

  basic_ht_hash_t h1 = 0, h2 = 0;
  basic_ht_hash (name, name_len, vhost_ht->seed, &h1, &h2);
  basic_ht_pair_t * pair = basic_ht_get_pair (vhost_ht, h1, h2);

  if (pair == NULL) return NULL;
  return (void*)pair->value2;
}

