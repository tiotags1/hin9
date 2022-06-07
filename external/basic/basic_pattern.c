/*
 * basic_libs, libraries used for other projects, including, pattern matching, timers and others
 * written by Alexandru C
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at: docs/LICENSE.txt
 * documentation is in the docs folder
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <stdarg.h>

#include "basic_pattern.h"

enum {RUN_ONE = 0x0, RUN_ALL=0x10, RUN_ZERO=0x1};

static inline char get_specifier (int x) {
  if (x == '+') return RUN_ALL;
  if (x == '*') return RUN_ALL|RUN_ZERO;
  if (x == '?') return RUN_ONE|RUN_ZERO;
  return RUN_ONE;
}

static inline int match_char (const char * ptr, uint32_t matches, uint32_t ch) {
  if (matches & PATTERN_CUSTOM 		&& *ptr == ch) return 1;
  if (matches & PATTERN_SPACE 		&& isspace (*ptr)) return 1;
  if (matches & PATTERN_ALPHANUM 	&& isalnum (*ptr)) return 1;
  if (matches & PATTERN_LETTER 		&& isalpha (*ptr)) return 1;
  if (matches & PATTERN_DIGIT 		&& isdigit (*ptr)) return 1;
  if (matches & PATTERN_CONTROL 	&& iscntrl (*ptr)) return 1;
  if (matches & PATTERN_PUNCTUATION	&& ispunct (*ptr)) return 1;
  if (matches & PATTERN_LOWER 		&& islower (*ptr)) return 1;
  if (matches & PATTERN_UPPER 		&& isupper (*ptr)) return 1;
  if (matches & PATTERN_HEXA 		&& isxdigit (*ptr)) return 1;
  if (matches & PATTERN_NULL_CHAR	&& *ptr == '\0') return 1;
  if (matches & PATTERN_NEGATIVE	&& *ptr != ch) return 1;
  if (matches & PATTERN_ALL 		&& *ptr != '\0') return 1;
  return 0;
}

static inline int get_pattern (const char * fmt, uint32_t * matches, uint32_t * character) {
  if (*fmt != '%') {
    *matches = PATTERN_CUSTOM;
    *character = *(fmt);
    return 1;
  }
  fmt++;
  switch (*fmt) {
  case 'a': *matches =  PATTERN_LETTER;		break; // letters
  case 'A': *matches = ~PATTERN_LETTER; 	break; // not all non letters

  case 's': *matches =  PATTERN_SPACE;		break; // spaces
  case 'S': *matches = ~PATTERN_SPACE;		break; // not spaces

  case 'c': *matches =  PATTERN_CONTROL;	break; // control characters
  case 'C': *matches = ~PATTERN_CONTROL;	break; // not control characters

  case 'd': *matches =  PATTERN_DIGIT;		break; // digits
  case 'D': *matches = ~PATTERN_DIGIT;		break; // not digits

  case 'l': *matches =  PATTERN_LOWER;		break; // lower case
  case 'L': *matches = ~PATTERN_LOWER;		break; // not lower case

  case 'u': *matches =  PATTERN_UPPER;		break; // punctuation
  case 'U': *matches = ~PATTERN_UPPER;		break; // not punctuation

  case 'p': *matches =  PATTERN_PUNCTUATION;	break; // uppercase
  case 'P': *matches = ~PATTERN_PUNCTUATION;	break; // not uppercase

  case 'w': *matches =  PATTERN_ALPHANUM;	break; // alpha numeric
  case 'W': *matches = ~PATTERN_ALPHANUM;	break; // not alpha numeric

  case 'x': *matches =  PATTERN_HEXA;		break; // hexa numbers
  case 'X': *matches = ~PATTERN_HEXA;		break; // not hexa numbers

  case 'z': *matches =  PATTERN_NULL_CHAR;	break; // /0
  case 'Z': *matches = ~PATTERN_NULL_CHAR; 	break; // not /0

  case 'y': *matches =  PATTERN_ALL;		break; // TODO more testing
  case 'Y': *matches = ~PATTERN_ALL;		break; // TODO more testing

  default:
    // add only the specific char to the buffer
    *matches = PATTERN_CUSTOM;
    *character = *fmt;
  break;
  }
  return 2;
}

static inline int match_pattern (const string_t *data, const string_t *pattern, int specifier) {
  const char * fmt;
  const char * start_fmt = pattern->ptr;
  const char * max_fmt = pattern->ptr + pattern->len;
  const char * str = data->ptr;
  const char * max_str = data->ptr + data->len;
  uint32_t matches, character = 0;
  int match_anything, match_negative = 0;
  if (*start_fmt == '^') {
    match_negative = 1;
    start_fmt++;
  }
  while (1) {
    match_anything = 0;
    fmt = start_fmt;
    while (fmt < max_fmt) {
      fmt += get_pattern (fmt, &matches, &character);
      if (str < max_str && match_char (str, matches, character)) {
        str++;
        match_anything = 1;
      } else {
        continue;
      }
      if (specifier & RUN_ALL) {
        while (str < max_str && match_char (str, matches, character)) {
          str++;
        }
      }
    }
    if (match_negative) {
      if (match_anything) { str--; break; }
      if (str >= max_str) break;
      str++;
    } else {
      if (match_anything == 0) { break; }
    }
  }
  size_t count = str-data->ptr;
  if (count == 0 && (specifier & RUN_ZERO) == 0) {
    return -1;
  }
  return count;
}

int match_string_virtual (string_t * data, uint32_t flags, const char *format, va_list argptr) {
  if (data->len <= 0) {
    if (*format == '\0') return 0;
    return -1;
  }
  string_t pattern, string;
  int used = 0;
  uint32_t specifier;
  const char * fmt = format;
  const char * ptr = data->ptr;
  const char * max = data->ptr + data->len;
  const char * start_capture = NULL;
  char error_char = 0;
  while (1) {
do_next:
    switch (*fmt) {
    case '%':
      pattern.ptr = (char*)(fmt++);
      fmt++;
      specifier = get_specifier (*fmt);
      pattern.len = fmt-pattern.ptr;
      if (specifier != RUN_ONE) fmt++;

      string.ptr = (char*)ptr;
      string.len = max - ptr;

      used = match_pattern (&string, &pattern, specifier);
      if (used >= 0) { ptr += used; }
      else { return -1; }

      goto do_next;
    break;
    case '[':
      pattern.ptr = (char*)(++fmt);
      while (1) {
        if (*fmt == '\0') {
          error_char = ']';
          goto finalize;
        }
        if (*fmt == ']' && *(fmt-1) != '%') {
          break;
        }
        fmt++;
      }
      pattern.len = fmt - pattern.ptr;
      fmt++;

      specifier = get_specifier (*fmt);
      if (specifier != RUN_ONE) fmt++;

      string.ptr = (char*)ptr;
      string.len = max - ptr;

      used = match_pattern (&string, &pattern, specifier);
      if (used >= 0) { ptr += used; }
      else { return -1; }

      goto do_next;
    break;
    case '(':
      start_capture = ptr;
    break;
    case ')': {
      string_t * out = va_arg (argptr, string_t*);
      if (start_capture) {
        out->ptr = (char*)start_capture;
        out->len = ptr-start_capture;
        start_capture = NULL;
      } else {
        error_char = '(';
        goto finalize;
      }
    break; }
    case '\0':
      goto finalize;
    break;
    default:
      if (ptr >= max) { return -1; }
      if (flags & PATTERN_CASE) {
        if (toupper (*fmt) != toupper (*ptr)) { return -1; }
      } else {
        if (*fmt != *ptr) { return -1; }
      }
      ptr++;
    break;
    }
    fmt++;
  }
finalize:
  if (start_capture) {
    error_char = ')';
  }
  if (error_char) {
    fprintf (stderr, "error! missing '%c' in format '%s'\n", error_char, format);
    return -1;
  }
  used = ptr - data->ptr;
  if (used < 0) return -1;
  data->ptr += used;
  data->len -= used;
  return used;
}

int match_string (string_t * source, const char *format, ...) {
  va_list argptr;
  va_start (argptr, format);

  int used = match_string_virtual (source, 0, format, argptr);

  va_end (argptr);
  return used;
}

int matchi_string (string_t * source, const char *format, ...) {
  va_list argptr;
  va_start (argptr, format);

  int used = match_string_virtual (source, PATTERN_CASE, format, argptr);

  va_end (argptr);
  return used;
}

int match_string_equal (string_t * source, const char * format, ...) {
  va_list argptr;
  va_start (argptr, format);
  string_t orig = *source;

  int used = match_string_virtual (source, 0, format, argptr);

  va_end (argptr);

  *source = orig;
  if (used != orig.len) {
    return -1;
  }

  return used;
}

int matchi_string_equal (string_t * source, const char * format, ...) {
  va_list argptr;
  va_start (argptr, format);
  string_t orig = *source;

  int used = match_string_virtual (source, PATTERN_CASE, format, argptr);

  va_end (argptr);

  *source = orig;
  if (used != orig.len) {
    return -1;
  }

  return used;
}



