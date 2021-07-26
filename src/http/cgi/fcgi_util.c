
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "hin.h"
#include "http.h"

#include <basic_endianness.h>

#include "fcgi.h"

FCGI_Header * hin_fcgi_header (hin_buffer_t * buf, int type, int id, int sz) {
  FCGI_Header * head = header_ptr (buf, sizeof (*head));
  head->version = FCGI_VERSION_1;
  head->type = type;
  head->request_id = endian_swap16 (id);
  head->length = endian_swap16 (sz);
  head->padding = 0;
  if (buf->debug & DEBUG_CGI)
    printf ("fcgi type %d req %d sz %d\n", type, id, sz);
  return head;
}



