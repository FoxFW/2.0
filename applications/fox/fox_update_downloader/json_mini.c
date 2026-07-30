#include "json_mini.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char* find_value_start(const char* json, const char* key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(json, pattern);
    if(!p) return NULL;
    p += strlen(pattern);
    const char* colon = strchr(p, ':');
    if(!colon) return NULL;
    p = colon + 1;
    while(*p == ' ' || *p == '\t') p++;
    return p;
}

bool json_mini_get_string(const char* json, const char* key, char* out, size_t out_size) {
    if(!json || !key || !out || out_size == 0) return false;
    const char* p = find_value_start(json, key);
    if(!p) return false;

    size_t n = 0;
    if(*p == '"') {
        p++;
        while(*p && *p != '"' && n + 1 < out_size) {
            if(*p == '\\' && *(p + 1)) p++;
            out[n++] = *p++;
        }
    } else {
        while(*p && *p != ',' && *p != '}' && *p != ']' && *p != '\n' && *p != '\r' &&
              n + 1 < out_size) {
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
    return true;
}

bool json_mini_get_uint(const char* json, const char* key, uint32_t* out) {
    char buf[24];
    if(!json_mini_get_string(json, key, buf, sizeof(buf))) return false;
    char* end = NULL;
    unsigned long v = strtoul(buf, &end, 10);
    if(end == buf) return false;
    *out = (uint32_t)v;
    return true;
}
