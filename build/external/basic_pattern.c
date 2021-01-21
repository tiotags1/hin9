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
  if (matches & PATTERN_ALL 		&& *ptr != '\n' && *(ptr) != '\0') { return 1; }
  return 0;
}

static inline int get_pattern (const char * fmt, uint32_t * matches, uint32_t * character) {
  switch (*fmt) {
  case '.':
    *matches = PATTERN_ALL;
    *character = '.';
    break;
  case '%':
      switch (*(fmt+1)) {
      case 'a': *matches = PATTERN_LETTER;	break; // letters
      case 's': *matches = PATTERN_SPACE;	break; // spaces
      case 'c': *matches = PATTERN_CONTROL;	break; // control characters
      case 'd': *matches = PATTERN_DIGIT;	break; // digits
      case 'l': *matches = PATTERN_LOWER;	break; // lower case
      case 'u': *matches = PATTERN_UPPER;	break; // punctuation
      case 'p': *matches = PATTERN_PUNCTUATION;	break; // uppercase
      case 'w': *matches = PATTERN_ALPHANUM;	break; // alpha numeric
      case 'x': *matches = PATTERN_HEXA;	break; // hexa numbers
      case 'z': *matches = PATTERN_NULL_CHAR;	break; // /0
      case 'A': *matches = ~PATTERN_LETTER; 	break; // not all non letters
      case 'S': *matches = ~PATTERN_SPACE;	break; // not spaces
      case 'C': *matches = ~PATTERN_CONTROL;	break; // not control characters
      case 'D': *matches = ~PATTERN_DIGIT;	break; // not digits
      case 'L': *matches = ~PATTERN_LOWER;	break; // not lower case
      case 'U': *matches = ~PATTERN_UPPER;	break; // not punctuation
      case 'P': *matches = ~PATTERN_PUNCTUATION;break; // not uppercase
      case 'W': *matches = ~PATTERN_ALPHANUM;	break; // not alpha numeric
      case 'X': *matches = ~PATTERN_HEXA;	break; // not hexa numbers
      case 'Z': *matches = ~PATTERN_NULL_CHAR; 	break; // not /0
      default:
        // add only the specific char to the buffer
        *matches = PATTERN_CUSTOM;
        *character = *(fmt+1);
        break;
      }
      return 2;
      break;
  case '^':
    *matches = PATTERN_NEGATIVE; break;
    *character = *(fmt+1);
    return 2;
    break;
  default:
    *matches = PATTERN_CUSTOM;
    *character = *(fmt);
    break;
  }
  return 1;
}

static inline int match_pattern (const string_t *data, const string_t *pattern, int specifier) {
  // for every pattern try to see it works
  const char * s = data->ptr, * test_s;
  const char * fmt;
  const char * max_s = data->ptr+data->len, * max_f = pattern->ptr+pattern->len;
  int match_anything, negative;
  uint32_t matches, character;
  if (specifier != '*' && specifier != '+') max_s = data->ptr + 1;
  do {
    match_anything = 0;
    for (fmt = pattern->ptr; fmt<max_f; ) {
      if (*fmt == '^') { negative = 1; fmt++; } else { negative = 0; }
      fmt += get_pattern (fmt, &matches, &character);
      test_s = s;

      switch (specifier) {
      case '+': // one or more as many as possible
        if (!(match_char (s, matches, character) ^ negative)) { break; }
        s++;
        while (s < max_s && (match_char (s, matches, character) ^ negative)) { s++; }
        break;
      case '*': // as many characters as possible but also nil
        while (s < max_s && (match_char (s, matches, character) ^ negative)) { s++; }
        break;
      case '?': // one or nil
        if (s<max_s && (match_char (s, matches, character) ^ negative)) { s++; }
        break;
      default: // one or fail
        if (s<max_s && (match_char (s, matches, character) ^ negative)) { s++; }
        break;
      }

      if ((s != test_s)) {
        match_anything = 1;
      }
    }
  } while (match_anything);
  size_t count = s-data->ptr;
  if (count == 0 && specifier != '*' && specifier != '?') {
    return -1;
  }
  return count;
}

static inline int get_specifier (int x) {
  if (x == '+' || x == '*' || x == '?') { return x; }
  return 0;
}

int match_string_virtual (string_t *data, uint32_t flags, const char *format, va_list argptr) {
  size_t count = 0;
  int specifier;
  const char * s;
  const char * fmt, * old_fmt;
  const char * max = data->ptr + data->len;
  const char * start_data = NULL, * end_data = NULL;
  const char * start_pattern = NULL, * end_pattern = NULL;
  string_t string = *data;
  string_t pattern;
  for (s=data->ptr, fmt = format; s<=max && *fmt; fmt++) {
    old_fmt = fmt;
    switch (*fmt) {

    case '[': {
      start_pattern = fmt+1;
      while (*fmt && *fmt != ']') fmt++;
      end_pattern = fmt;
      int specifier = *(fmt+1);
      switch (specifier) {
      case '+':
      case '*':
      case '?':
        fmt++;
      }
      pattern.ptr = (char*)start_pattern;
      pattern.len = end_pattern-start_pattern;
      string.ptr = (char*)s;
      string.len = max - s;
      int used = match_pattern (&string, &pattern, specifier);
      //printf ("pattern used %d %.*s\n",used,pattern.len,pattern.ptr);
      s += used;
      break;
    }

    case '(':
      start_data = s;
      break;

    case ')': {
      string_t * str = va_arg (argptr, string_t*);
      if (start_data == NULL) {
        //basic_error ("TODO why does this start without a matching (");
        str->ptr = NULL;
        str->len = 0;
      } else {
        str->ptr = (char*)start_data;
        str->len = s-start_data;
      }
      break;
    }

    case '%': fmt++;
    case '.': {
      specifier = 0;
      pattern.ptr = (char*)old_fmt;
      pattern.len = fmt-old_fmt+1;
      switch (*(fmt+1)) {
      case '+':
      case '*':
      case '?':
        fmt++;
        specifier = *fmt;
        break;
      }
      string.ptr = (char*)s;
      string.len = max - s;
      int used = match_pattern (&string, &pattern, specifier);
      //printf ("searching str %.*s for '%.*s' fmt_len %d used %d\n",string.len,string.ptr,pattern.len,pattern.ptr,pattern.len,used);
      if (used >= 0) {
        s += used;
      } else return -1;
      break;
    }

    default: {
      if (get_specifier (*(fmt+1)) == 0) {
        if (*fmt == 0) return -1;
        if (flags & PATTERN_CASE) {
          if (toupper (*fmt) != toupper(*s)) return -1;
        } else {
          if (*fmt != *s) return -1;
        }
        s++;
      } else {
        specifier = *(fmt+1);
        pattern.ptr = (char*)fmt;
        pattern.len = 1;
        string.ptr = (char*)s;
        string.len = max-s;
        fmt ++;
        int used = match_pattern (&string, &pattern, specifier);
        if (used >= 0) {
          s += used;
        } else return -1;
      }
    }
  }
  }

  //if (*fmt != '\0') return -1;
  intptr_t used = s - data->ptr;
  if (used < 0) return -1;
  data->ptr += used;
  data->len -= used;
  return used;
}


int match_string (string_t *data, const char *format, ...) {
  va_list argptr;
  va_start (argptr, format);

  int used = match_string_virtual (data, 0, format, argptr);

  va_end (argptr);
  return used;
}

int matchi_string (string_t *data, const char *format, ...) {
  va_list argptr;
  va_start (argptr, format);

  int used = match_string_virtual (data, PATTERN_CASE, format, argptr);

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

char * match_string_to_c (string_t * str) {
  return strndup (str->ptr, str->len);
}

string_t match_c_to_string (const char * ptr) {
  string_t str;
  str.ptr = (char*)ptr;
  str.len = strlen (ptr);
  return str;
}


