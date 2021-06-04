
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basic_hashtable.h>

#include "hin.h"
#include "hin_lua.h"

static basic_ht_t * vhost_ht = NULL;

void hin_vhost_clean () {
  if (vhost_ht)
    basic_ht_free (vhost_ht);
  vhost_ht = NULL;
}

int hin_vhost_add (const char * name, int name_len, hin_vhost_t * vhost) {
  if (vhost_ht == NULL) {
    vhost_ht = basic_ht_create (1024, 101);
  }

  basic_ht_hash_t h1 = 0, h2 = 0;
  basic_ht_hash (name, name_len, vhost_ht->seed, &h1, &h2);
  basic_ht_set_pair (vhost_ht, h1, h2, 0, (uintptr_t)vhost);

  return 0;
}

hin_vhost_t * hin_vhost_get (const char * name, int name_len) {
  if (vhost_ht == NULL) return NULL;

  basic_ht_hash_t h1 = 0, h2 = 0;
  basic_ht_hash (name, name_len, vhost_ht->seed, &h1, &h2);
  basic_ht_pair_t * pair = basic_ht_get_pair (vhost_ht, h1, h2);

  if (pair == NULL) return NULL;
  return (void*)pair->value2;
}

#include "http.h"

int httpd_vhost_switch (httpd_client_t * http, hin_vhost_t * vhost) {
  http->vhost = vhost;
  http->debug = vhost->debug;
  http->disable = vhost->disable;
  void httpd_client_ping (httpd_client_t * http, int timeout);
  httpd_client_ping (http, vhost->timeout);
  return 0;
}

int httpd_vhost_request (httpd_client_t * http, const char * name, int len) {
  if (http->hostname) {
    if (strncmp (http->hostname, name, len) == 0 && len == strlen (http->hostname))
      return 0;
    return -1;
  }

  http->hostname = strndup (name, len);

  hin_vhost_t * vhost = hin_vhost_get (name, len);
  if (http->debug & (DEBUG_HTTP|DEBUG_RW_ERROR))
    printf ("hostname '%.*s'%s\n", len, name, vhost ? "" : " not found");

  if (vhost) {
    httpd_vhost_switch (http, vhost);
    return 0;
  }

  return -1;
}


