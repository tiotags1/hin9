
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "conf.h"

#ifdef HIN_USE_OPENSSL

// Obtain the return value of an SSL operation and convert into a simplified
// error code, which is easier to examine for failure.
enum sslstatus { SSL_STATUS_OK=1, SSL_STATUS_WANT_READ, SSL_STATUS_WANT_WRITE, SSL_STATUS_FAIL};

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
  default: printf ("SSL_UNKNOWN_ERROR\n"); break;
  fail:
    if (err == SSL_ERROR_SSL)
      fprintf (stderr, "ssl err: %s\n", ERR_error_string (ERR_get_error (), NULL));
    return SSL_STATUS_FAIL;
  }
  return 0;
}

static int hin_ssl_handshake (hin_ssl_t * ssl, hin_buffer_t * crypt);

int hin_ssl_callback (hin_buffer_t * plain, int ret) {
  if (plain->callback (plain, ret)) {
    hin_buffer_clean (plain);
  }
  return 1;
}

int hin_ssl_read_callback (hin_buffer_t * crypt, int ret) {
  hin_ssl_t * ssl = crypt->ssl;
  ssl->flags &= ~HIN_SSL_READ;

  if (ret <= 0) {
    if (crypt->debug & DEBUG_SSL) printf ("ssl closed %p\n", crypt);
    for (hin_buffer_t * plain = ssl->read; plain; plain = plain->next) {
      plain->ssl_buffer = NULL;
      hin_ssl_callback (plain, ret);
    }
    ssl->read = NULL;
    crypt->parent = NULL;
    return 1;
  }

  int n = BIO_write (ssl->rbio, crypt->ptr, ret);
  if (crypt->debug & DEBUG_SSL) printf ("ssl done read %d/%d/%d\n", n, ret, crypt->count);

  if (n < 0) {
    printf ("ssl bio read fatal fail %d<%d\n", n, ret);
    return 1;
    //return hin_ssl_callback (plain, ret);
  }

  hin_ssl_handshake (ssl, crypt);
  return 0;
}

static int hin_ssl_write_callback (hin_buffer_t * crypt, int ret) {
  hin_ssl_t * ssl = crypt->ssl;
  ssl->flags &= ~HIN_SSL_WRITE;

  if (ret < crypt->count) {
    if (crypt->debug & DEBUG_SSL) printf ("ssl incomplete write done callback %d/%d bytes\n", ret, crypt->count);
    if (ret < 0) { printf ("ssl write error '%s'\n", strerror (-ret)); return -1; }
    crypt->ptr += ret;
    crypt->count -= ret;
    if (hin_request_write (crypt) < 0) {
      hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;
      hin_ssl_callback (plain, -EPROTO);
    }
    return 0;
  }

  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;
  if (plain) {
    if (crypt->debug & DEBUG_SSL) printf ("ssl done write crypt %d/%d plain %d\n", ret, crypt->count, plain->count);
    plain->ssl_buffer = NULL;
    hin_ssl_callback (plain, plain->count);
  } else {
    if (crypt->debug & DEBUG_SSL) printf ("ssl done write crypt %d/%d\n", ret, crypt->count);
  }
  hin_ssl_handshake (crypt->ssl, crypt);
  return 0;
}

static int hin_ssl_do_read (hin_ssl_t * ssl, hin_buffer_t * crypt) {
  hin_buffer_t * plain = ssl->read;
  if (plain == NULL) return 0;

  if (ssl->flags & HIN_SSL_READ) return 0;
  ssl->flags |= HIN_SSL_READ;

  int m = SSL_read (ssl->ssl, plain->ptr, plain->count);
  if (m > 0) {
    if (crypt->debug & DEBUG_SSL) printf ("ssl read ahead read %d plain %d\n", m, plain->count);
    ssl->flags &= ~HIN_SSL_READ;
    hin_buffer_list_remove ((hin_buffer_t**)&ssl->read, plain);
    return hin_ssl_callback (plain, m); // does clean inside the function
  }

  if (crypt->debug & DEBUG_SSL) printf ("ssl read request\n");

  crypt->parent = plain;
  crypt->count = crypt->sz;
  crypt->callback = hin_ssl_read_callback;
  plain->ssl_buffer = crypt;

  if (hin_request_read (crypt) < 0) {
    hin_ssl_callback (plain, -EPROTO);
    return -1;
  }

  return 1;
}

static int hin_ssl_do_write (hin_ssl_t * ssl, hin_buffer_t * crypt) {
  hin_buffer_t * plain = ssl->write;
  if (plain == NULL) return 0;

  if (ssl->flags & HIN_SSL_WRITE) return 0;
  ssl->flags |= HIN_SSL_WRITE;

  hin_buffer_list_remove ((hin_buffer_t**)&ssl->write, plain);

  int m = SSL_write (ssl->ssl, plain->ptr, plain->count);
  if (m < 0) {
    printf ("ssl write fail %d < %d\n", m, plain->count);
    hin_ssl_callback (plain, -EPROTO);
    return -1;
  }

  int n = BIO_read (ssl->wbio, crypt->ptr, crypt->count);
  if (n < 0) {
    printf ("ssl bio write fatal fail %d\n", n);
    hin_ssl_callback (plain, -EPROTO);
    return -1; // if BIO write fails, assume unrecoverable
  }

  if (m < plain->count) { printf ("ssl write to buffer incomplete %d/%d\n", m, plain->count); }
  if (crypt->debug & DEBUG_SSL) printf ("ssl write plain %d/%d crypt %d/%d\n", m, plain->count, n, crypt->count);

  crypt->parent = plain;
  crypt->count = n;
  crypt->callback = hin_ssl_write_callback;
  plain->ssl_buffer = crypt;

  if (hin_request_write (crypt) < 0) {
    plain->ssl_buffer = NULL;
    hin_ssl_callback (plain, -EPROTO);
    return -1;
  }
  return 1;
}

static int hin_ssl_check_data (hin_ssl_t * ssl, hin_buffer_t * crypt) {
  int n;

  n = SSL_do_handshake (ssl->ssl);
  hin_buffer_t * plain = (hin_buffer_t*)crypt->parent;

  n = 0;
  if ((ssl->flags & HIN_SSL_WRITE) == 0)
    n = BIO_read (ssl->wbio, crypt->buffer, crypt->sz);
  if (n > 0) {
    if (crypt->debug & DEBUG_SSL) printf ("ssl want write %d\n", n);
    ssl->flags |= HIN_SSL_WRITE;
    crypt->count = n;
    crypt->callback = hin_ssl_write_callback;

    if (hin_request_write (crypt) < 0) {
      hin_ssl_callback (plain, -EPROTO);
    }
    return 1;
  } else if (n <= 0) {
    //printf ("ssl no bytes to queue\n");
  }

  n = SSL_do_handshake (ssl->ssl);

  int status = get_sslstatus (ssl->ssl, n);
  switch (status) {
  case SSL_STATUS_WANT_WRITE:
    printf ("ssl want write old ??? %p\n", crypt);
  break;
  case SSL_STATUS_WANT_READ:
    if (crypt->debug & DEBUG_SSL) printf ("ssl want read %d\n", crypt->sz);
    if (ssl->flags & HIN_SSL_READ) {
      hin_buffer_clean (crypt);
      return 1;
    }
    ssl->flags |= HIN_SSL_READ;
    crypt->callback = hin_ssl_read_callback;
    crypt->count = crypt->sz;

    if (hin_request_read (crypt) < 0) {
      hin_ssl_callback (plain, -EPROTO);
    }

    return 1;
  break;
  case SSL_STATUS_OK:
    //printf ("ssl ok old ???\n");
  break;
  default: printf ("ssl fail default\n");
    // fall through
  case SSL_STATUS_FAIL:
    printf ("ssl status fail\n");
    hin_buffer_clean (crypt);
    return -1;
  break;
  }

  if ((n = hin_ssl_do_write (ssl, crypt))) {
    if (n < 0) printf ("ssl error do write %s\n", strerror (-n));
    return n;
  }
  if ((n = hin_ssl_do_read (ssl, crypt))) {
    if (n < 0) printf ("ssl error do read %s\n", strerror (-n));
    return n;
  }

  if (crypt->debug & DEBUG_SSL) printf ("ssl finished buffer %p\n", crypt);
  hin_buffer_clean (crypt);
  return 1;
}

static int hin_ssl_handshake (hin_ssl_t * ssl, hin_buffer_t * buf) {
  hin_buffer_t * crypt = buf;
  if (buf->flags & HIN_SSL && buf->ssl_buffer) {
    crypt = buf->ssl_buffer;
  } else if (buf->flags & HIN_SSL) {
    int sz = READ_SZ + 100;
    crypt = malloc (sizeof (hin_buffer_t) + sz);
    memset (crypt, 0, sizeof (hin_buffer_t));
    crypt->flags = buf->flags & (~HIN_SSL);
    crypt->fd = buf->fd;
    crypt->count = crypt->sz = sz;
    crypt->ptr = crypt->buffer;
    crypt->callback = hin_ssl_read_callback;
    crypt->ssl = ssl;
    if (buf) {
      crypt->debug = buf->debug;
    } else {
      crypt->debug = master.debug;
    }
  }
  crypt->parent = NULL;

  int n;
  while (1) {
    n = hin_ssl_check_data (ssl, crypt);
    if (n) return n;
  }
  return -1;
}

int hin_ssl_request_write (hin_buffer_t * buffer) {
  hin_ssl_t * ssl = buffer->ssl;
  if (buffer->prev || buffer->next) { printf ("ERROR! buffer is already part of a list\n"); }
  hin_buffer_list_append ((hin_buffer_t**)&ssl->write, buffer);
  if (ssl->write == buffer) hin_ssl_handshake (ssl, buffer);
  return 0;
}

int hin_ssl_request_read (hin_buffer_t * buffer) {
  hin_ssl_t * ssl = buffer->ssl;
  if (buffer->prev || buffer->next) { printf ("ERROR! buffer is already part of a list\n"); }
  hin_buffer_list_append ((hin_buffer_t**)&ssl->read, buffer);
  if (ssl->read == buffer) hin_ssl_handshake (ssl, buffer);
  return 0;
}

#endif
