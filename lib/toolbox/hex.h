#pragma once
#include "stdint.h"
#include "stdbool.h"

#ifdef __cplusplus
extern "C" {
#endif

bool hex_char_to_hex_nibble(char c, uint8_t* nibble);

bool hex_char_to_uint8(char hi, char low, uint8_t* value);

bool hex_chars_to_uint8(const char* value_str, uint8_t* value);

bool hex_chars_to_uint64(const char* value_str, uint64_t* value);

void uint8_to_hex_chars(const uint8_t* src, uint8_t* target, int length);

#ifdef __cplusplus
}
#endif
