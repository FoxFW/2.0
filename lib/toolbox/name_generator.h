#pragma once

#include <stdint.h>
#include <stddef.h>
#include <furi_hal_rtc.h>

#ifdef __cplusplus
extern "C" {
#endif

void name_generator_make_auto(char* name, size_t max_name_size, const char* prefix);
void name_generator_make_auto_datetime(
    char* name,
    size_t max_name_size,
    const char* prefix,
    DateTime* custom_time);

void name_generator_make_auto_basic(char* name, size_t max_name_size, const char* prefix);

void name_generator_make_random(char* name, size_t max_name_size);
void name_generator_make_random_prefixed(char* name, size_t max_name_size, const char* prefix);

void name_generator_make_detailed(char* name, size_t max_name_size, const char* prefix);
void name_generator_make_detailed_datetime(
    char* name,
    size_t max_name_size,
    const char* prefix,
    DateTime* custom_time);

#ifdef __cplusplus
}
#endif
