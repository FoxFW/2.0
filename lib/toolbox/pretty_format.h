#pragma once

#include <core/string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRETTY_FORMAT_FONT_BOLD      "\e#"
#define PRETTY_FORMAT_FONT_MONOSPACE "\e*"

void pretty_format_bytes_hex_canonical(
    FuriString* result,
    size_t num_places,
    const char* line_prefix,
    const uint8_t* data,
    size_t data_size);

#ifdef __cplusplus
}
#endif
