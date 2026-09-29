#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t varint_uint32_pack(uint32_t value, uint8_t* output);

size_t varint_uint32_unpack(uint32_t* value, const uint8_t* input, size_t input_size);

size_t varint_uint32_length(uint32_t value);

size_t varint_int32_pack(int32_t value, uint8_t* output);

size_t varint_int32_unpack(int32_t* value, const uint8_t* input, size_t input_size);

size_t varint_int32_length(int32_t value);

#ifdef __cplusplus
}
#endif
