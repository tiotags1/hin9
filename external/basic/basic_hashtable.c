/*
 * basic_libs, libraries used for other projects, including, pattern matching, timers and others
 * written by Alexandru C
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at: docs/LICENSE.txt
 * documentation is in the docs folder
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "basic_hashtable.h"

static int isPowerOfTwo (int num) {
  if (num <= 0) return 0;
  //return ((num & (num-1)) == 0); // have not audited this
  int pow = 1;
  while (num > pow) {
    pow = pow * 2;
  }
  if (num == pow) return 1;
  return 0;
}

int basic_ht_init (basic_ht_t * ht, int size, basic_ht_hash_t seed) {
  if (size < 1) return -1;
  if (size > 0xffff) printf ("warning hashtable is too large\n");

  if (!isPowerOfTwo (size)) { printf ("ERROR hashtable init, %d not power of 2\n", size); return -1; }
  ht->size = size;
  ht->mask = size-1;
  ht->seed = seed;

  // Allocate pointers to the head nodes.
  if ((ht->table = calloc (1, sizeof (basic_ht_pair_t *) * size)) == NULL) {
    return -1;
  }
  return 0;
}

// Create a new hashtable.
basic_ht_t * basic_ht_create (int size, basic_ht_hash_t seed) {
  basic_ht_t * ht = calloc (1, sizeof (*ht));

  if (basic_ht_init (ht, size, seed) < 0) { free (ht); return NULL; }

  return ht;
}

void basic_ht_clean (basic_ht_t * hashtable) {
  basic_ht_pair_t * pair, * pair_old;
  for (int i=0; i<hashtable->size; i++) {
    pair = hashtable->table [i];
    while (pair) {
      pair_old = pair;
      pair = pair->next;
      free (pair_old);
    }
  }
  free (hashtable->table);
}

void basic_ht_free (basic_ht_t * hashtable) {
  basic_ht_clean (hashtable);
  free (hashtable);
}

basic_ht_hash_t basic_ht_hash (const char * str, size_t size, basic_ht_hash_t seed, basic_ht_hash_t * h1, basic_ht_hash_t * h2) {
  basic_ht_hash_t hash1 = 5381 * seed;
  basic_ht_hash_t hash2 = 6883 * seed;
  int c;
  const char * max = str+size;
  while (str < max) {
    c = *str++;
    hash1 = ((hash1 << 5) + hash1) + c; // hash * 33 + c
    hash2 = ((hash2 << 3) + hash2) + c;
  }
  if (h2) *h2 = hash2;
  if (h1) *h1 = hash1;
  return hash1;
}

basic_ht_hash_t basic_ht_hash_continue (const char * str, size_t size, basic_ht_hash_t * h1, basic_ht_hash_t * h2) {
  basic_ht_hash_t hash1 = *h1;
  basic_ht_hash_t hash2 = *h2;
  int c;
  const char * max = str+size;
  while (str < max) {
    c = *str++;
    hash1 = ((hash1 << 5) + hash1) + c; // hash * 33 + c
    hash2 = ((hash2 << 3) + hash2) + c;
  }
  if (h2) *h2 = hash2;
  if (h1) *h1 = hash1;
  return hash1;
}

basic_ht_pair_t * basic_ht_get_pair (basic_ht_t *hashtable, basic_ht_hash_t key1, basic_ht_hash_t key2) {
  basic_ht_hash_t bin = key1 & hashtable->mask;
  basic_ht_pair_t * pair;

  // Step through the bin, looking for our value.
  pair = hashtable->table [bin];
  while (pair != NULL) {
    if (pair->key1 == key1 && pair->key2 == key2) return pair;
    pair = pair->next;
  }
  return NULL;
}

// Insert a key-value pair into a hash table.
int basic_ht_set_pair (basic_ht_t * hashtable, basic_ht_hash_t key1, basic_ht_hash_t key2, basic_ht_hash_t value1, basic_ht_hash_t value2) {
  basic_ht_pair_t * next = NULL, * last = NULL, * newpair;

  basic_ht_hash_t bin = key1 & hashtable->mask;
  next = hashtable->table [bin];
  while (next) {
    if (next->key1 == key1 && next->key2 == key2) {
      next->value1 = value1;
      next->value2 = value2;
      return 0;
    }
    last = next;
    next = next->next;
  }

  newpair = calloc (1, sizeof (basic_ht_pair_t));
  newpair->key1 = key1;
  newpair->key2 = key2;
  newpair->value1 = value1;
  newpair->value2 = value2;
  //newpair->next = NULL;

  if (last == NULL) {
    hashtable->table [bin] = newpair;
  } else {
    last->next = newpair;
  }
  return 0;
}

int basic_ht_delete_pair (basic_ht_t *hashtable, basic_ht_hash_t key1, basic_ht_hash_t key2) {
  basic_ht_pair_t * next = NULL, * last = NULL;

  basic_ht_hash_t bin = key1 & hashtable->mask;
  next = hashtable->table [bin];
  while (next) {
    if (next->key1 == key1 && next->key2 == key2) {
      if (last) {
        last->next = next->next;
      } else {
        hashtable->table[bin] = next->next;
      }
      free (next);
      return 1;
    }
    last = next;
    next = next->next;
  }
  return 0;
}

basic_ht_pair_t * basic_ht_iterate_pair (basic_ht_t * hashtable, basic_ht_iterator_t * iter) {
  if (iter->pair && iter->pair->next) {
    iter->pair = iter->pair->next;
    return iter->pair;
  }
  for (basic_ht_hash_t i=iter->bin; i<hashtable->size; i++) {
    iter->pair = hashtable->table[i];
    if (iter->pair) {
      iter->bin = i+1;
      return iter->pair;
    }
  }
  return 0;
}




