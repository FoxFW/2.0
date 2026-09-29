#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    StrintParseNoError,
    StrintParseSignError,
    StrintParseAbsentError,
    StrintParseOverflowError,
} StrintParseError;

StrintParseError strint_to_uint64(const char* str, char** end, uint64_t* out, uint8_t base);

StrintParseError strint_to_int64(const char* str, char** end, int64_t* out, uint8_t base);

StrintParseError strint_to_uint32(const char* str, char** end, uint32_t* out, uint8_t base);

StrintParseError strint_to_int32(const char* str, char** end, int32_t* out, uint8_t base);

StrintParseError strint_to_uint16(const char* str, char** end, uint16_t* out, uint8_t base);

StrintParseError strint_to_int16(const char* str, char** end, int16_t* out, uint8_t base);

#ifdef __cplusplus
}
#endif
