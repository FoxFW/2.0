#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

bool json_mini_get_string(const char* json, const char* key, char* out, size_t out_size);

bool json_mini_get_uint(const char* json, const char* key, uint32_t* out);

const char* json_object_end(const char* obj_start);

#define JSON_FILE_READER_CHUNK 128

typedef struct {
    File* file;
    char chunk[JSON_FILE_READER_CHUNK];
    size_t chunk_len;
    size_t chunk_pos;
} JsonFileReader;

void json_file_reader_init(JsonFileReader* reader, File* file);
bool json_file_skip_to(JsonFileReader* reader, const char* needle);
bool json_file_reader_next_object(JsonFileReader* reader, char* obj_buf, size_t obj_buf_size);
