#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdint.h>

typedef uintptr_t basic_ht_hash_t;

typedef struct basic_ht_pair_struct {
  basic_ht_hash_t key1;
  basic_ht_hash_t key2;
  basic_ht_hash_t value1;
  basic_ht_hash_t value2;
  struct basic_ht_pair_struct *next;
} basic_ht_pair_t;

typedef struct basic_ht_struct {
  int size;
  basic_ht_hash_t seed;
  basic_ht_hash_t mask;
  basic_ht_pair_t ** table;
} basic_ht_t;

typedef struct {
  basic_ht_hash_t bin;
  basic_ht_pair_t * pair;
} basic_ht_iterator_t;


basic_ht_t * basic_ht_create (int size, basic_ht_hash_t seed);
// returns 0 for no problem and non 0 for errors
int basic_ht_init (basic_ht_t * ht, int size, basic_ht_hash_t seed);
void basic_ht_clean (basic_ht_t * ht);
void basic_ht_free (basic_ht_t * ht);

basic_ht_hash_t basic_ht_hash (const char * str, size_t size, basic_ht_hash_t seed, basic_ht_hash_t * h1, basic_ht_hash_t * h2);
basic_ht_hash_t basic_ht_hash_continue (const char * str, size_t size, basic_ht_hash_t * h1, basic_ht_hash_t * h2);

// base
basic_ht_pair_t * basic_ht_get_pair (basic_ht_t * ht, basic_ht_hash_t key1, basic_ht_hash_t key2);
int basic_ht_set_pair (basic_ht_t * ht, basic_ht_hash_t key1, basic_ht_hash_t key2, basic_ht_hash_t value1, basic_ht_hash_t value2);
int basic_ht_delete_pair (basic_ht_t * ht, basic_ht_hash_t key1, basic_ht_hash_t key2);

basic_ht_pair_t * basic_ht_iterate_pair (basic_ht_t * ht, basic_ht_iterator_t * iter);

#endif
