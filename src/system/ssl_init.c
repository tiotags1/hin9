
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <liburing.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>

#include <hin.h>

// Global SSL context
SSL_CTX * default_ctx = NULL;

int hin_accept_ssl_init (hin_client_t * client) {
  hin_ssl_t * ssl = &client->ssl;
  hin_client_t * server = (hin_client_t*)client->parent;
  hin_server_blueprint_t * bp = (hin_server_blueprint_t*)&server->extra;

  ssl->rbio = BIO_new (BIO_s_mem ());
  ssl->wbio = BIO_new (BIO_s_mem ());

  ssl->ssl = SSL_new (bp->ssl_ctx);

  SSL_set_accept_state (ssl->ssl); // sets ssl to work in server mode.
  SSL_set_bio (ssl->ssl, ssl->rbio, ssl->wbio);

  client->flags |= HIN_SSL;
  if (master.debug & DEBUG_SSL) printf ("ssl init accept sockfd %d\n", client->sockfd);
}

int hin_connect_ssl_init (hin_client_t * client) {
  hin_ssl_t * ssl = &client->ssl;

  ssl->rbio = BIO_new (BIO_s_mem ());
  ssl->wbio = BIO_new (BIO_s_mem ());

  ssl->ssl = SSL_new (default_ctx);

  SSL_set_connect_state (ssl->ssl); // sets ssl to work in connect mode.
  SSL_set_bio (ssl->ssl, ssl->rbio, ssl->wbio);

  client->flags |= HIN_SSL;
  if (master.debug & DEBUG_SSL) printf ("ssl init connect sockfd %d\n", client->sockfd);
}

void hin_client_ssl_cleanup (hin_client_t * client) {
  hin_ssl_t * ssl = &client->ssl;
  SSL_free (ssl->ssl);   // free the SSL object and its BIO's
  //free (http->write_buf);
  //free (http->encrypt_buf);
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
  err = SSL_CTX_use_certificate_file (ctx, cert,  SSL_FILETYPE_PEM);
  if (err != 1) {
    printf ("SSL_CTX_use_certificate_file failed\n");
    return NULL;
  } else {
    printf("ssl certificate file loaded ok.\n");
  }

  // Indicate the key file to be used
  err = SSL_CTX_use_PrivateKey_file (ctx, key, SSL_FILETYPE_PEM);
  if (err != 1) {
    printf ("SSL_CTX_use_PrivateKey_file failed\n");
    return NULL;
  } else {
    printf("ssl private-key file loaded ok.\n");
  }

  // Make sure the key and certificate file match.
  if (SSL_CTX_check_private_key (ctx) != 1) {
    printf ("SSL_CTX_check_private_key failed");
    return NULL;
  } else {
    printf("ssl private key verified ok.\n");
  }

  //if (SSL_CTX_set_max_proto_version (ctx, TLS1_2_VERSION) == 0) {
  //  printf ("can't set max proto version\n");
  //}
  //SSL_CTX_set_options (ctx, SSL_OP_ALL|SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);

  // Recommended to avoid SSLv2 & SSLv3
  //SSL_CTX_set_options (ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);
  return ctx;
}



