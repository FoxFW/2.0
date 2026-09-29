#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BitBuffer BitBuffer;

BitBuffer* bit_buffer_alloc(size_t capacity_bytes);

void bit_buffer_free(BitBuffer* buf);

void bit_buffer_reset(BitBuffer* buf);

void bit_buffer_copy(BitBuffer* buf, const BitBuffer* other);

void bit_buffer_copy_right(BitBuffer* buf, const BitBuffer* other, size_t start_index);

void bit_buffer_copy_left(BitBuffer* buf, const BitBuffer* other, size_t end_index);

void bit_buffer_copy_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes);

void bit_buffer_copy_bits(BitBuffer* buf, const uint8_t* data, size_t size_bits);

void bit_buffer_copy_bytes_with_parity(BitBuffer* buf, const uint8_t* data, size_t size_bits);

void bit_buffer_write_bytes(const BitBuffer* buf, void* dest, size_t size_bytes);

void bit_buffer_write_bytes_with_parity(
    const BitBuffer* buf,
    void* dest,
    size_t size_bytes,
    size_t* bits_written);

void bit_buffer_write_bytes_mid(
    const BitBuffer* buf,
    void* dest,
    size_t start_index,
    size_t size_bytes);

bool bit_buffer_has_partial_byte(const BitBuffer* buf);

bool bit_buffer_starts_with_byte(const BitBuffer* buf, uint8_t byte);

size_t bit_buffer_get_capacity_bytes(const BitBuffer* buf);

size_t bit_buffer_get_size(const BitBuffer* buf);

size_t bit_buffer_get_size_bytes(const BitBuffer* buf);

uint8_t bit_buffer_get_byte(const BitBuffer* buf, size_t index);

uint8_t bit_buffer_get_byte_from_bit(const BitBuffer* buf, size_t index_bits);

const uint8_t* bit_buffer_get_data(const BitBuffer* buf);

const uint8_t* bit_buffer_get_parity(const BitBuffer* buf);

void bit_buffer_set_byte(BitBuffer* buf, size_t index, uint8_t byte);

void bit_buffer_set_byte_with_parity(BitBuffer* buff, size_t index, uint8_t byte, bool parity);

void bit_buffer_set_size(BitBuffer* buf, size_t new_size);

void bit_buffer_set_size_bytes(BitBuffer* buf, size_t new_size_bytes);

void bit_buffer_append(BitBuffer* buf, const BitBuffer* other);

void bit_buffer_append_right(BitBuffer* buf, const BitBuffer* other, size_t start_index);

void bit_buffer_append_byte(BitBuffer* buf, uint8_t byte);

void bit_buffer_append_bytes(BitBuffer* buf, const uint8_t* data, size_t size_bytes);

void bit_buffer_append_bit(BitBuffer* buf, bool bit);

#ifdef __cplusplus
}
#endif
