#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

bool args_read_int_and_trim(FuriString* args, int* value);

bool args_read_float_and_trim(FuriString* args, float* value);

bool args_read_string_and_trim(FuriString* args, FuriString* word);

bool args_read_probably_quoted_string_and_trim(FuriString* args, FuriString* word);

bool args_read_hex_bytes(FuriString* args, uint8_t* bytes, size_t bytes_count);

bool args_read_duration(FuriString* args, uint32_t* value, const char* default_unit);

size_t args_get_first_word_length(FuriString* args);

size_t args_length(FuriString* args);

bool args_char_to_hex(char hi_nibble, char low_nibble, uint8_t* byte);

#ifdef __cplusplus
}
#endif
