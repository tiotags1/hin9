
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"

// Obtain the return value of an SSL operation and convert into a simplified
// error code, which is easier to examine for failure.
enum sslstatus { SSL_STATUS_OK, SSL_STATUS_WANT_READ, SSL_STATUS_WANT_WRITE, SSL_STATUS_FAIL};

static enum sslstatus get_sslstatus (SSL* ssl, int n) {
  int err = SSL_get_error (ssl, n);
  switch (err) {
  case SSL_ERROR_NONE:
      return SSL_STATUS_OK;
  case SSL_ERROR_WANT_WRITE:
    return SSL_STATUS_WANT_WRITE;
  case SSL_ERROR_WANT_READ:
    return SSL_STATUS_WANT_READ;
  case SSL_ERROR_ZERO_RETURN: printf ("SSL_ERROR_ZERO_RETURN\n"); goto fail;
  case SSL_ERROR_WANT_CONNECT: printf ("SSL_ERROR_WANT_CONNECT\n"); goto fail;
  case SSL_ERROR_WANT_ACCEPT: printf ("SSL_ERROR_WANT_ACCEPT\n"); goto fail;
  case SSL_ERROR_WANT_X509_LOOKUP: printf ("SSL_ERROR_WANT_X509_LOOKUP\n"); goto fail;
  case SSL_ERROR_SYSCALL: printf ("SSL_ERROR_SYSCALL\n"); goto fail;
  case SSL_ERROR_SSL: printf ("SSL_ERROR_SSL\n"); goto fail;
  default:
  fail:
    if (err == SSL_ERROR_SSL)
      fprintf (stderr, "ssl err: %s\n", ERR_error_string (ERR_get_error (), NULL));
    return SSL_STATUS_FAIL;
  }
}

int hin_ssl_handshake (hin_ssl_t * ssl, hin_buffer_t * crypt);
int hin_ssl_read (hin_buffer_t * crypt, int ret);
int hin_ssl_write (hin_buffer_t * crypt);
int ssl_init_write_ready (hin_buffer_t * buffer, int ret);

static int ssl_init_complete (hin_buffer_t * crypt) {
  if (master.debug & DEBUG_SSL) printf ("ssl init ok\n");
  switch ((uint8_t)crypt->data) {
  case HIN_SSL_READ:
    crypt->callback = hin_ssl_read;
    crypt->count = crypt->sz;
    hin_request_read (crypt);
  break;
  case HIN_SSL_WRITE:
    hin_ssl_write (crypt);
    hin_request_write (crypt);
  break;
  default: printf ("error"); break;
  }
}

int hin_ssl_flush_write (hin_buffer_t * crypt) {
  while (1) {
    int n = BIO_read (crypt->ssl->wbio, crypt->buffer, crypt->sz);
    if (n > 0) {
      if (master.debug & DEBUG_SSL) printf ("ssl init want write %d\n", n);
      crypt->count = n;
      crypt->callback = ssl_init_write_ready;
      hin_request_write (crypt);
      return 1;
    } else if (!BIO_should_retry (crypt->ssl->wbio)) {
      printf ("shouldn't retry ?\n");
      return -1;
    } else if (n <= 0) {
      //printf ("ssl no bytes to queue\n");
      break;
    }
  }
  return 0;
}

int ssl_init_read_ready (hin_buffer_t * crypt, int ret) {
  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;
  hin_ssl_t * ssl = plain->ssl;

  if (ret == 0) {
    if (master.debug & DEBUG_SSL) printf ("ssl init detect close\n");
    plain->ssl_buffer = NULL;
    if (plain->callback (plain, ret))
      hin_buffer_clean (plain);
    return 1;
  }

  int n = BIO_write (ssl->rbio, crypt->ptr, ret);
  if (master.debug & DEBUG_SSL) printf ("ssl init read %d/%d/%d\n", n, ret, crypt->count);

  if (n < ret) {
    printf ("ssl bio read fatal fail %d<%d\n", n, ret);
    return -1; // if BIO write fails, assume unrecoverable
  }

  if (hin_ssl_flush_write (crypt)) {
    return 0;
  }

  if (hin_ssl_handshake (ssl, crypt)) {
    return 0;
  }
  ssl_init_complete (crypt);
  return 0;
}

int ssl_init_write_ready (hin_buffer_t * crypt, int ret) {
  if (master.debug & DEBUG_SSL) printf ("ssl init write %d/%d\n", ret, crypt->count);
  // TODO handle incomplete write for when server is overloaded ?
  if (hin_ssl_handshake (crypt->ssl, crypt)) {
    return 0;
  }
  ssl_init_complete (crypt);
  return 0;
}

int hin_ssl_handshake (hin_ssl_t * ssl, hin_buffer_t * crypt) {
  int n, status;
  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;

  if (SSL_is_init_finished (ssl->ssl)) { return 0; }
  int min_sz = 4096;
  if (crypt->sz < min_sz) {
    hin_buffer_t * new = malloc (sizeof (hin_buffer_t) + min_sz);
    *new = *crypt;
    new->count = new->sz = min_sz;
    new->ptr = new->buffer;
    hin_buffer_clean (crypt);
    crypt = new;
  }

  while (1) {
    n = SSL_do_handshake (ssl->ssl);
    status = get_sslstatus (ssl->ssl, n);

    if (hin_ssl_flush_write (crypt)) return 1;

    switch (status) {
    case SSL_STATUS_WANT_WRITE:
      printf ("ssl want write old ???\n");
    break;
    case SSL_STATUS_WANT_READ:
      crypt->callback = ssl_init_read_ready;
      crypt->count = crypt->sz;
      if (master.debug & DEBUG_SSL) printf ("ssl init want read %d\n", crypt->count);
      hin_request_read (crypt);
      return 1;
    break;
    case SSL_STATUS_FAIL:
      printf ("ssl status fail\n");
      return -1;
    break;
    case SSL_STATUS_OK:
      //printf ("ssl init ok old ???\n");
      return 0;
    break;
    }
  }

  return 1;
}

int hin_ssl_read (hin_buffer_t * crypt, int ret) {
  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;
  hin_ssl_t * ssl = plain->ssl;

  if (ret == 0) {
    if (master.debug & DEBUG_SSL) printf ("ssl detect close\n");
    plain->ssl_buffer = NULL;
    if (plain->callback (plain, ret))
      hin_buffer_clean (plain);
    return 1;
  }

  int n = BIO_write (ssl->rbio, crypt->ptr, ret);
  if (master.debug & DEBUG_SSL) printf ("ssl bio read %d/%d/%d\n", n, ret, crypt->count);

  if (n < ret) {
    printf ("ssl bio read fatal fail %d<%d\n", n, ret);
    return -1; // if BIO write fails, assume unrecoverable
  }

  if (hin_ssl_handshake (ssl, crypt)) {
    return 0;
  }

  int m = SSL_read (ssl->ssl, plain->ptr, plain->count);
  if (m < 0) {
    printf ("ssl read fail %d < %d\n", m, plain->count);
    return -1;
  }

  plain->ssl_buffer = NULL;
  if (plain->callback (plain, m))
    hin_buffer_clean (plain);
  return 1;
}

static int ssl_write_done (hin_buffer_t * crypt, int ret) {
  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;
  if (ret < crypt->count) {
    if (master.debug & DEBUG_SSL) printf ("ssl incomplete write done callback %d/%d bytes\n", ret, crypt->count);
    crypt->ptr += ret;
    crypt->count -= ret;
    hin_request_write (crypt);
    return 0;
  }

  plain->ssl_buffer = NULL;
  if (plain->callback (plain, plain->count))
    hin_buffer_clean (plain);
  return 1;
}

int hin_ssl_write (hin_buffer_t * crypt) {
  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;
  hin_ssl_t * ssl = plain->ssl;

  if (hin_ssl_handshake (ssl, crypt)) {
    return 0;
  }

  int m = SSL_write (ssl->ssl, plain->ptr, plain->count);
  if (m < 0) {
    printf ("ssl write fail %d < %d\n", m, plain->count);
    return -1;
  }

  int n = BIO_read (ssl->wbio, crypt->ptr, crypt->count);
  if (n < 0) {
    printf ("ssl bio write fatal fail %d\n", n);
    return -1; // if BIO write fails, assume unrecoverable
  }

  if (master.debug & DEBUG_SSL) printf ("ssl bio write %d/%d/%d\n", m, n, crypt->count);
  crypt->count = n;
  crypt->callback = ssl_write_done;

  return 0;
}



