#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define bit_read(value, bit) (((value) >> (bit)) & 0x01)
#define bit_set(value, bit)           \
    ({                                \
        __typeof__(value) _one = (1); \
        (value) |= (_one << (bit));   \
    })
#define bit_clear(value, bit)         \
    ({                                \
        __typeof__(value) _one = (1); \
        (value) &= ~(_one << (bit));  \
    })
#define bit_write(value, bit, bitvalue) (bitvalue ? bit_set(value, bit) : bit_clear(value, bit))
#define DURATION_DIFF(x, y)             (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

#ifdef __cplusplus
extern "C" {
#endif

uint64_t subghz_protocol_blocks_reverse_key(uint64_t key, uint8_t bit_count);

uint8_t subghz_protocol_blocks_get_parity(uint64_t key, uint8_t bit_count);

uint8_t subghz_protocol_blocks_crc4(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init);

uint8_t subghz_protocol_blocks_crc7(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init);

uint8_t subghz_protocol_blocks_crc8(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init);

uint8_t subghz_protocol_blocks_crc8le(
    uint8_t const message[],
    size_t size,
    uint8_t polynomial,
    uint8_t init);

uint16_t subghz_protocol_blocks_crc16lsb(
    uint8_t const message[],
    size_t size,
    uint16_t polynomial,
    uint16_t init);

uint16_t subghz_protocol_blocks_crc16(
    uint8_t const message[],
    size_t size,
    uint16_t polynomial,
    uint16_t init);

uint8_t subghz_protocol_blocks_lfsr_digest8(
    uint8_t const message[],
    size_t size,
    uint8_t gen,
    uint8_t key);

uint8_t subghz_protocol_blocks_lfsr_digest8_reflect(
    uint8_t const message[],
    size_t size,
    uint8_t gen,
    uint8_t key);

uint16_t subghz_protocol_blocks_lfsr_digest16(
    uint8_t const message[],
    size_t size,
    uint16_t gen,
    uint16_t key);

uint8_t subghz_protocol_blocks_add_bytes(uint8_t const message[], size_t size);

uint8_t subghz_protocol_blocks_parity8(uint8_t byte);

uint8_t subghz_protocol_blocks_parity_bytes(uint8_t const message[], size_t size);

uint8_t subghz_protocol_blocks_xor_bytes(uint8_t const message[], size_t size);

#ifdef __cplusplus
}
#endif
