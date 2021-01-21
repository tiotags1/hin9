#ifndef BASIC_PATTERN_H
#define BASIC_PATTERN_H

#include <stdint.h>

// 2 types of strings, interned and buffers

enum {PATTERN_NIL=0, PATTERN_LETTER=1, PATTERN_SPACE=2, PATTERN_CONTROL=4, PATTERN_DIGIT=8, PATTERN_LOWER=0x10, PATTERN_UPPER=0x20, PATTERN_PUNCTUATION=0x40, PATTERN_ALPHANUM=0x80, PATTERN_HEXA=0x100, PATTERN_NULL_CHAR=0x200, PATTERN_CUSTOM=0x400, PATTERN_NEGATIVE=0x800, PATTERN_ALL=0x1000};

enum { PATTERN_CASE = 0x1 };

typedef struct {
  char * ptr;
  uintptr_t len;
} string_t;

int match_string_virtual (string_t *data, uint32_t flags, const char *format, va_list argptr);

int match_string (string_t *data, const char *format, ...);
int matchi_string (string_t *data, const char *format, ...);

int match_string_equal (string_t * source, const char * format, ...);
int matchi_string_equal (string_t * source, const char * format, ...);

char * match_string_to_c (string_t * str);
string_t match_c_to_string (const char * ptr);

#endif
