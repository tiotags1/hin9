
#ifndef HIN_SSL_H
#define HIN_SSL_H

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

enum { HIN_SSL_READ = 1, HIN_SSL_WRITE };

typedef struct {
  SSL *ssl;
  BIO *rbio;	// SSL reads from, we write to
  BIO *wbio;	// SSL writes to, we read from
} hin_ssl_t;

int hin_connect_ssl_init (hin_client_t * client);
int hin_accept_ssl_init (hin_client_t * client);

#endif

