
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <liburing.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>

#include "hin.h"
#include "hin_lua.h"

#ifdef HIN_USE_OPENSSL

// Global SSL context
SSL_CTX * default_ctx = NULL;

SSL_CTX * hin_ssl_default_init () {
  SSL_CTX * ctx = NULL;

  SSL_library_init ();
  OpenSSL_add_all_algorithms ();
  SSL_load_error_strings ();
  ERR_load_BIO_strings ();
  ERR_load_crypto_strings ();

  const SSL_METHOD *method = TLS_method ();

  ctx = SSL_CTX_new (method);

  if (ctx == NULL) {
    perror ("Unable to create SSL context");
    return NULL;
  }

  SSL_CTX_set_ecdh_auto (ctx, 1);

  return ctx;
}

int hin_ssl_accept_init (hin_client_t * client) {
  hin_ssl_t * ssl = &client->ssl;
  hin_server_t * server = (hin_server_t*)client->parent;

  ssl->rbio = BIO_new (BIO_s_mem ());
  ssl->wbio = BIO_new (BIO_s_mem ());

  ssl->ssl = SSL_new (server->ssl_ctx);

  SSL_set_accept_state (ssl->ssl); // sets ssl to work in server mode.
  SSL_set_bio (ssl->ssl, ssl->rbio, ssl->wbio);

  client->flags |= HIN_SSL;
  if (master.debug & DEBUG_SSL) printf ("ssl init accept sockfd %d\n", client->sockfd);
  return 1;
}

int hin_ssl_connect_init (hin_client_t * client) {
  hin_ssl_t * ssl = &client->ssl;

  ssl->rbio = BIO_new (BIO_s_mem ());
  ssl->wbio = BIO_new (BIO_s_mem ());

  if (default_ctx == NULL) {
    default_ctx = hin_ssl_default_init ();
  }

  ssl->ssl = SSL_new (default_ctx);

  SSL_set_connect_state (ssl->ssl); // sets ssl to work in connect mode.
  SSL_set_bio (ssl->ssl, ssl->rbio, ssl->wbio);

  client->flags |= HIN_SSL;
  if (master.debug & DEBUG_SSL) printf ("ssl init connect sockfd %d\n", client->sockfd);
  return 1;
}

void hin_client_ssl_cleanup (hin_client_t * client) {
  hin_ssl_t * ssl = &client->ssl;
  SSL_free (ssl->ssl);   // free the SSL object and its BIO's
}

void hin_ssl_print_error () {
  int err1 = ERR_get_error ();
  printf ("ssl error reason '%s'\n", ERR_reason_error_string (err1));
  printf ("ssl error '%s'\n", ERR_error_string (err1, NULL));
}

static int hin_ssl_sni_callback (SSL *ssl, int *ad, void *arg) {
  if (ssl == NULL)
    return SSL_TLSEXT_ERR_NOACK;

  const char* servername = SSL_get_servername (ssl, TLSEXT_NAMETYPE_host_name);
  if (servername == NULL || servername[0] == '\0') {
    printf ("ssl SNI null\n");
    return SSL_TLSEXT_ERR_NOACK;
  }

  if (master.debug & HIN_SSL)
    printf ("ssl SNI '%s'\n", servername);

  hin_vhost_t * vhost = hin_vhost_get (servername, strlen (servername));
  if (vhost == NULL || vhost->ssl_ctx == NULL) {
    printf ("ssl can't find vhost '%s'\n", servername);
    return SSL_TLSEXT_ERR_OK;
  }
  SSL_CTX * r = SSL_set_SSL_CTX (ssl, vhost->ssl_ctx);
  if (r != vhost->ssl_ctx) {
    printf ("ssl can't set new ctx\n");
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  return SSL_TLSEXT_ERR_OK;
}

SSL_CTX * hin_ssl_init (const char * cert, const char * key) {
  SSL_CTX * ctx = NULL;

  SSL_library_init ();
  OpenSSL_add_all_algorithms ();
  SSL_load_error_strings ();
  ERR_load_BIO_strings ();
  ERR_load_crypto_strings ();

  const SSL_METHOD *method = TLS_method ();

  ctx = SSL_CTX_new (method);

  if (ctx == NULL) {
    perror ("Unable to create SSL context");
    return NULL;
  }

  SSL_CTX_set_ecdh_auto (ctx, 1);

  // Load certificate and private key files, and check consistency
  int err;
  err = SSL_CTX_use_certificate_file (ctx, cert, SSL_FILETYPE_PEM);
  if (err != 1) {
    printf ("SSL_CTX_use_certificate_file '%s' failed\n", cert);
    hin_ssl_print_error ();
    return NULL;
  }
  if (master.debug & HIN_SSL)
    printf ("ssl cert '%s' ok.\n", cert);

  // Indicate the key file to be used
  err = SSL_CTX_use_PrivateKey_file (ctx, key, SSL_FILETYPE_PEM);
  if (err != 1) {
    printf ("SSL_CTX_use_PrivateKey_file '%s' failed\n", key);
    hin_ssl_print_error ();
    return NULL;
  }
  if (master.debug & HIN_SSL)
    printf("ssl key '%s' ok.\n", key);

  // Make sure the key and certificate file match.
  if (SSL_CTX_check_private_key (ctx) != 1) {
    printf ("SSL_CTX_check_private_key failed");
    hin_ssl_print_error ();
    return NULL;
  }

  SSL_CTX_set_tlsext_servername_callback (ctx, hin_ssl_sni_callback);

  // Recommended to avoid SSLv2 & SSLv3
  SSL_CTX_set_options (ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);
  return ctx;
}

#else

int hin_ssl_accept_init (hin_client_t * client) {
  printf ("openssl disabled\n");
  return -1;
}

int hin_ssl_connect_init (hin_client_t * client) {
  printf ("openssl disabled\n");
  return -1;
}

void hin_client_ssl_cleanup (hin_client_t * client) {}

void * hin_ssl_init (const char * cert, const char * key) {
  printf ("openssl disabled\n");
  return NULL;
}

int hin_ssl_request_write (hin_buffer_t * buffer) { return -1; }
int hin_ssl_request_read (hin_buffer_t * buffer) { return -1; }

#endif

