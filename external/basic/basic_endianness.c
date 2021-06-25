/*
 * basic_libs, libraries used for other projects, including, pattern matching, timers and others
 * written by Alexandru C
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at: docs/LICENSE.txt
 * documentation is in the docs folder
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "basic_endianness.h"

int basic_detect_little_endian () {
  static const int val = 1;
  return *(const uint8_t *)&val != 0;
}

uint16_t endian_swap16 (uint16_t n) {
  return (n<<8) | (n>>8);
}

uint32_t endian_swap32 (uint32_t n) {
  return (n<<24) | (n>>24) | ((n>>8)&0xFF00) | ((n<<8)&0xFF0000);
}

uint64_t endian_swap64 (uint64_t x) {
  x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
  x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
  x = (x & 0x00FF00FF00FF00FF) << 8  | (x & 0xFF00FF00FF00FF00) >> 8;
  return x;
}

uint16_t little_endian_swap16 (uint16_t n) {
  if (basic_detect_little_endian ()) return n;
  else return endian_swap16 (n);
}

uint32_t little_endian_swap32 (uint32_t n) {
  if (basic_detect_little_endian ()) return n;
  else return endian_swap32 (n);
}

uint64_t little_endian_swap64 (uint64_t n) {
  if (basic_detect_little_endian ()) return n;
  else return endian_swap64 (n);
}

void little_endian_buffer16 (uint16_t *buf, int count) {
  int i, n;
  if (!basic_detect_little_endian ()) {
    for (i=0;i<count;i++) {
      n = buf[i];
      buf[i] = endian_swap16(n);
    }
  }
}

void little_endian_buffer32 (uint32_t *buf, int count) {
  int i, n;
  if (!basic_detect_little_endian ()) {
    for (i=0;i<count;i++) {
      n = buf[i];
      buf[i] = endian_swap32(n);
    }
  }
}

void little_endian_buffer64 (uint64_t *buf, int count) {
  int i, n;
  if (!basic_detect_little_endian ()) {
    for (i=0;i<count;i++) {
      n = buf[i];
      buf[i] = endian_swap64 (n);
    }
  }
}

void big_endian_buffer16 (uint16_t *buf, int count) {
  int i, n;
  if (basic_detect_little_endian ()) {
    for (i=0;i<count;i++) {
      n = buf[i];
      buf[i] = endian_swap16(n);
    }
  }
}

void big_endian_buffer32 (uint32_t *buf, int count) {
  int i, n;
  if (basic_detect_little_endian ()) {
    for (i=0;i<count;i++) {
      n = buf[i];
      buf[i] = endian_swap32(n);
    }
  }
}

void big_endian_buffer64 (uint64_t *buf, int count) {
  int i, n;
  if (basic_detect_little_endian ()) {
    for (i=0;i<count;i++) {
      n = buf[i];
      buf[i] = endian_swap64 (n);
    }
  }
}

