#include "strutil.h"

#include <string.h>
#include <ctype.h>

void str_copy(char* dst, size_t dst_size, const char* src) {
    if(dst_size == 0) return;
    size_t len = strlen(src);
    if(len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

void str_join2(char* dst, size_t dst_size, const char* a, const char* b) {
    if(dst_size == 0) return;
    size_t len_a = strlen(a);
    if(len_a >= dst_size) len_a = dst_size - 1;
    memcpy(dst, a, len_a);
    size_t remaining = dst_size - len_a - 1;
    size_t len_b = strlen(b);
    if(len_b > remaining) len_b = remaining;
    memcpy(dst + len_a, b, len_b);
    dst[len_a + len_b] = '\0';
}

void str_join3(char* dst, size_t dst_size, const char* a, const char* b, const char* c) {
    if(dst_size == 0) return;
    size_t len_a = strlen(a);
    if(len_a >= dst_size) len_a = dst_size - 1;
    memcpy(dst, a, len_a);
    size_t pos = len_a;

    size_t remaining = dst_size - pos - 1;
    size_t len_b = strlen(b);
    if(len_b > remaining) len_b = remaining;
    memcpy(dst + pos, b, len_b);
    pos += len_b;

    remaining = dst_size - pos - 1;
    size_t len_c = strlen(c);
    if(len_c > remaining) len_c = remaining;
    memcpy(dst + pos, c, len_c);
    pos += len_c;

    dst[pos] = '\0';
}

void str_capitalize_first(char* s) {
    if(!s) return;

    while(*s == ' ') s++;
    if(*s) {
        *s = (char)toupper((unsigned char)*s);
    }
}
