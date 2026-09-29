#pragma once
#include <stdint.h>
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FlipperFormat FlipperFormat;

typedef enum {
    FlipperFormatOffsetFromCurrent,
    FlipperFormatOffsetFromStart,
    FlipperFormatOffsetFromEnd,
} FlipperFormatOffset;

FlipperFormat* flipper_format_string_alloc(void);

FlipperFormat* flipper_format_file_alloc(Storage* storage);

FlipperFormat* flipper_format_buffered_file_alloc(Storage* storage);

bool flipper_format_file_open_existing(FlipperFormat* flipper_format, const char* path);

bool flipper_format_buffered_file_open_existing(FlipperFormat* flipper_format, const char* path);

bool flipper_format_file_open_append(FlipperFormat* flipper_format, const char* path);

bool flipper_format_file_open_always(FlipperFormat* flipper_format, const char* path);

bool flipper_format_buffered_file_open_always(FlipperFormat* flipper_format, const char* path);

bool flipper_format_file_open_new(FlipperFormat* flipper_format, const char* path);

bool flipper_format_file_close(FlipperFormat* flipper_format);

bool flipper_format_buffered_file_close(FlipperFormat* flipper_format);

void flipper_format_free(FlipperFormat* flipper_format);

void flipper_format_set_strict_mode(FlipperFormat* flipper_format, bool strict_mode);

bool flipper_format_rewind(FlipperFormat* flipper_format);

size_t flipper_format_tell(FlipperFormat* flipper_format);

bool flipper_format_seek(FlipperFormat* flipper_format, int32_t offset, FlipperFormatOffset anchor);

bool flipper_format_seek_to_end(FlipperFormat* flipper_format);

bool flipper_format_key_exist(FlipperFormat* flipper_format, const char* key);

bool flipper_format_read_header(
    FlipperFormat* flipper_format,
    FuriString* filetype,
    uint32_t* version);

bool flipper_format_write_header(
    FlipperFormat* flipper_format,
    FuriString* filetype,
    const uint32_t version);

bool flipper_format_write_header_cstr(
    FlipperFormat* flipper_format,
    const char* filetype,
    const uint32_t version);

bool flipper_format_get_value_count(
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t* count);

bool flipper_format_read_string(FlipperFormat* flipper_format, const char* key, FuriString* data);

bool flipper_format_write_string(FlipperFormat* flipper_format, const char* key, FuriString* data);

bool flipper_format_write_string_cstr(
    FlipperFormat* flipper_format,
    const char* key,
    const char* data);

bool flipper_format_read_hex_uint64(
    FlipperFormat* flipper_format,
    const char* key,
    uint64_t* data,
    const uint16_t data_size);

bool flipper_format_write_hex_uint64(
    FlipperFormat* flipper_format,
    const char* key,
    const uint64_t* data,
    const uint16_t data_size);

bool flipper_format_read_uint32(
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t* data,
    const uint16_t data_size);

bool flipper_format_write_uint32(
    FlipperFormat* flipper_format,
    const char* key,
    const uint32_t* data,
    const uint16_t data_size);

bool flipper_format_read_int32(
    FlipperFormat* flipper_format,
    const char* key,
    int32_t* data,
    const uint16_t data_size);

bool flipper_format_write_int32(
    FlipperFormat* flipper_format,
    const char* key,
    const int32_t* data,
    const uint16_t data_size);

bool flipper_format_read_bool(
    FlipperFormat* flipper_format,
    const char* key,
    bool* data,
    const uint16_t data_size);

bool flipper_format_write_bool(
    FlipperFormat* flipper_format,
    const char* key,
    const bool* data,
    const uint16_t data_size);

bool flipper_format_read_float(
    FlipperFormat* flipper_format,
    const char* key,
    float* data,
    const uint16_t data_size);

bool flipper_format_write_float(
    FlipperFormat* flipper_format,
    const char* key,
    const float* data,
    const uint16_t data_size);

bool flipper_format_read_hex(
    FlipperFormat* flipper_format,
    const char* key,
    uint8_t* data,
    const uint16_t data_size);

bool flipper_format_write_hex(
    FlipperFormat* flipper_format,
    const char* key,
    const uint8_t* data,
    const uint16_t data_size);

bool flipper_format_write_comment(FlipperFormat* flipper_format, FuriString* data);

bool flipper_format_write_comment_cstr(FlipperFormat* flipper_format, const char* data);

bool flipper_format_write_empty_line(FlipperFormat* flipper_format);

bool flipper_format_delete_key(FlipperFormat* flipper_format, const char* key);

bool flipper_format_update_string(FlipperFormat* flipper_format, const char* key, FuriString* data);

bool flipper_format_update_string_cstr(
    FlipperFormat* flipper_format,
    const char* key,
    const char* data);

bool flipper_format_update_uint32(
    FlipperFormat* flipper_format,
    const char* key,
    const uint32_t* data,
    const uint16_t data_size);

bool flipper_format_update_int32(
    FlipperFormat* flipper_format,
    const char* key,
    const int32_t* data,
    const uint16_t data_size);

bool flipper_format_update_bool(
    FlipperFormat* flipper_format,
    const char* key,
    const bool* data,
    const uint16_t data_size);

bool flipper_format_update_float(
    FlipperFormat* flipper_format,
    const char* key,
    const float* data,
    const uint16_t data_size);

bool flipper_format_update_hex(
    FlipperFormat* flipper_format,
    const char* key,
    const uint8_t* data,
    const uint16_t data_size);

bool flipper_format_insert_or_update_string(
    FlipperFormat* flipper_format,
    const char* key,
    FuriString* data);

bool flipper_format_insert_or_update_string_cstr(
    FlipperFormat* flipper_format,
    const char* key,
    const char* data);

bool flipper_format_insert_or_update_uint32(
    FlipperFormat* flipper_format,
    const char* key,
    const uint32_t* data,
    const uint16_t data_size);

bool flipper_format_insert_or_update_int32(
    FlipperFormat* flipper_format,
    const char* key,
    const int32_t* data,
    const uint16_t data_size);

bool flipper_format_insert_or_update_bool(
    FlipperFormat* flipper_format,
    const char* key,
    const bool* data,
    const uint16_t data_size);

bool flipper_format_insert_or_update_float(
    FlipperFormat* flipper_format,
    const char* key,
    const float* data,
    const uint16_t data_size);

bool flipper_format_insert_or_update_hex(
    FlipperFormat* flipper_format,
    const char* key,
    const uint8_t* data,
    const uint16_t data_size);

#ifdef __cplusplus
}
#endif
