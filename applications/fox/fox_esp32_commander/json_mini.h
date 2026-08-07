#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool json_mini_get_string(const char* json, const char* key, char* out, size_t out_size);

bool json_mini_get_uint(const char* json, const char* key, uint32_t* out);
