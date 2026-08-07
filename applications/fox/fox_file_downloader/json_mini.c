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

const char* json_object_end(const char* obj_start) {
    const char* p = obj_start + 1;
    int depth = 1;
    while(*p && depth > 0) {
        if(*p == '"') {
            p++;
            while(*p && *p != '"') {
                if(*p == '\\' && *(p + 1)) p++;
                p++;
            }
            if(*p == '"') p++;
            continue;
        }
        if(*p == '{') depth++;
        else if(*p == '}') depth--;
        p++;
    }
    return p;
}

void json_file_reader_init(JsonFileReader* reader, File* file) {
    reader->file = file;
    reader->chunk_len = 0;
    reader->chunk_pos = 0;
}

static bool reader_getc(JsonFileReader* reader, char* out) {
    if(reader->chunk_pos >= reader->chunk_len) {
        reader->chunk_len = storage_file_read(reader->file, reader->chunk, JSON_FILE_READER_CHUNK);
        reader->chunk_pos = 0;
        if(reader->chunk_len == 0) return false;
    }
    *out = reader->chunk[reader->chunk_pos++];
    return true;
}

bool json_file_skip_to(JsonFileReader* reader, const char* needle) {
    size_t needle_len = strlen(needle);
    size_t matched = 0;
    char c;
    while(reader_getc(reader, &c)) {
        if(c == needle[matched]) {
            matched++;
            if(matched == needle_len) return true;
        } else {
            matched = (c == needle[0]) ? 1 : 0;
        }
    }
    return false;
}

bool json_file_reader_next_object(JsonFileReader* reader, char* obj_buf, size_t obj_buf_size) {
    char c;
    for(;;) {
        if(!reader_getc(reader, &c)) return false;
        if(c == '{') break;
        if(c == ']') return false;
    }

    size_t n = 0;
    if(n + 1 < obj_buf_size) obj_buf[n++] = '{';
    int depth = 1;
    bool in_str = false;

    while(depth > 0) {
        if(!reader_getc(reader, &c)) break;

        if(in_str) {
            if(n + 1 < obj_buf_size) obj_buf[n++] = c;
            if(c == '\\') {
                char esc;
                if(reader_getc(reader, &esc) && n + 1 < obj_buf_size) obj_buf[n++] = esc;
                continue;
            }
            if(c == '"') in_str = false;
            continue;
        }

        if(c == '"') {
            in_str = true;
        } else if(c == '{') {
            depth++;
        } else if(c == '}') {
            depth--;
        }
        if(n + 1 < obj_buf_size) obj_buf[n++] = c;
    }

    obj_buf[n < obj_buf_size ? n : obj_buf_size - 1] = '\0';
    return true;
}
