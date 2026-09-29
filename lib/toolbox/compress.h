#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CompressIcon CompressIcon;

CompressIcon* compress_icon_alloc(size_t decode_buf_size);

void compress_icon_free(CompressIcon* instance);

void compress_icon_decode(CompressIcon* instance, const uint8_t* icon_data, uint8_t** output);

typedef struct Compress Compress;

typedef enum {
    CompressTypeHeatshrink = 0,
} CompressType;

typedef struct {
    uint16_t window_sz2;
    uint16_t lookahead_sz2;
    uint16_t input_buffer_sz;
} CompressConfigHeatshrink;

extern const CompressConfigHeatshrink compress_config_heatshrink_default;

Compress* compress_alloc(CompressType type, const void* config);

void compress_free(Compress* compress);

bool compress_encode(
    Compress* compress,
    uint8_t* data_in,
    size_t data_in_size,
    uint8_t* data_out,
    size_t data_out_size,
    size_t* data_res_size);

bool compress_decode(
    Compress* compress,
    uint8_t* data_in,
    size_t data_in_size,
    uint8_t* data_out,
    size_t data_out_size,
    size_t* data_res_size);

typedef int32_t (*CompressIoCallback)(void* context, uint8_t* buffer, size_t size);

bool compress_decode_streamed(
    Compress* compress,
    CompressIoCallback read_cb,
    void* read_context,
    CompressIoCallback write_cb,
    void* write_context);

typedef struct CompressStreamDecoder CompressStreamDecoder;

CompressStreamDecoder* compress_stream_decoder_alloc(
    CompressType type,
    const void* config,
    CompressIoCallback read_cb,
    void* read_context);

void compress_stream_decoder_free(CompressStreamDecoder* instance);

bool compress_stream_decoder_read(
    CompressStreamDecoder* instance,
    uint8_t* data_out,
    size_t data_out_size);

bool compress_stream_decoder_seek(CompressStreamDecoder* instance, size_t position);

size_t compress_stream_decoder_tell(CompressStreamDecoder* instance);

bool compress_stream_decoder_rewind(CompressStreamDecoder* instance);

#ifdef __cplusplus
}
#endif
