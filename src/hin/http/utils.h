
#ifndef HIN_UTILS_H
#define HIN_UTILS_H

int hin_client_addr (char * str, int len, struct sockaddr * ai_addr, socklen_t ai_addrlen);

#include <basic_pattern.h>

int hin_string_equali (string_t * source, const char * format, ...);
int find_line (string_t * source, string_t * line);

#endif

