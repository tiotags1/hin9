
#ifndef BASIC_ENDIAN_H
#define BASIC_ENDIAN_H

#include <stdint.h>
#include <stdlib.h> // for base64 size_t

// endian helpers
int basic_detect_little_endian ();

uint16_t endian_swap16 (uint16_t n);
uint32_t endian_swap32 (uint32_t n);
uint64_t endian_swap64 (uint64_t n);

uint16_t little_endian_swap16 (uint16_t n);
uint32_t little_endian_swap32 (uint32_t n);
uint64_t little_endian_swap64 (uint64_t n);

void little_endian_buffer16 (uint16_t * buf, int count);
void little_endian_buffer32 (uint32_t * buf, int count);
void little_endian_buffer64 (uint64_t * buf, int count);

#endif

