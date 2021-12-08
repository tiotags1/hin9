
#ifndef HIN_SSL_H
#define HIN_SSL_H

#include "config_int.h"

#ifdef HIN_USE_OPENSSL

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

enum { HIN_SSL_READ = 0x1, HIN_SSL_WRITE = 0x2 };

typedef struct {
  SSL *ssl;
  BIO *rbio;	// SSL reads from, we write to
  BIO *wbio;	// SSL writes to, we read from

  uint32_t flags;
  void * write, * read;
} hin_ssl_t;

#else

typedef struct {
} hin_ssl_t;

#endif

int hin_ssl_connect_init (hin_client_t * client);
int hin_ssl_accept_init (hin_client_t * client);
int hin_ssl_request_write (hin_buffer_t * buffer);
int hin_ssl_request_read (hin_buffer_t * buffer);

#endif

