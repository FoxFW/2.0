#pragma once

#include <stddef.h>

void str_copy(char* dst, size_t dst_size, const char* src);
void str_join2(char* dst, size_t dst_size, const char* a, const char* b);
void str_join3(char* dst, size_t dst_size, const char* a, const char* b, const char* c);

void str_capitalize_first(char* s);
