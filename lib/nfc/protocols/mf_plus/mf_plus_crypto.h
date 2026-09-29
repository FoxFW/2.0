#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_PLUS_AES_BLOCK_SIZE (16)
#define MF_PLUS_AES_KEY_SIZE   (16)
#define MF_PLUS_MAC_SIZE       (8)

void mf_plus_crypto_ecb_encrypt(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t input[MF_PLUS_AES_BLOCK_SIZE],
    uint8_t output[MF_PLUS_AES_BLOCK_SIZE]);

void mf_plus_crypto_ecb_decrypt(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t input[MF_PLUS_AES_BLOCK_SIZE],
    uint8_t output[MF_PLUS_AES_BLOCK_SIZE]);

void mf_plus_crypto_cbc_encrypt(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t iv[MF_PLUS_AES_BLOCK_SIZE],
    const uint8_t* input,
    uint8_t* output,
    size_t length);

void mf_plus_crypto_cbc_decrypt(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t iv[MF_PLUS_AES_BLOCK_SIZE],
    const uint8_t* input,
    uint8_t* output,
    size_t length);

void mf_plus_crypto_cmac(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t* data,
    size_t length,
    uint8_t mac[MF_PLUS_AES_BLOCK_SIZE]);

void mf_plus_crypto_cmac8(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t* data,
    size_t length,
    uint8_t mac[MF_PLUS_MAC_SIZE]);

void mf_plus_crypto_derive_session_keys(
    const uint8_t key[MF_PLUS_AES_KEY_SIZE],
    const uint8_t rnd_a[MF_PLUS_AES_BLOCK_SIZE],
    const uint8_t rnd_b[MF_PLUS_AES_BLOCK_SIZE],
    uint8_t k_enc[MF_PLUS_AES_KEY_SIZE],
    uint8_t k_mac[MF_PLUS_AES_KEY_SIZE]);

void mf_plus_crypto_calculate_mac(
    const uint8_t k_mac[MF_PLUS_AES_KEY_SIZE],
    uint8_t cmd,
    uint16_t counter,
    const uint8_t ti[4],
    const uint8_t* data,
    size_t data_length,
    uint8_t mac[MF_PLUS_MAC_SIZE]);

void mf_plus_crypto_build_read_iv(
    const uint8_t ti[4],
    uint16_t r_ctr,
    uint16_t w_ctr,
    uint8_t iv[MF_PLUS_AES_BLOCK_SIZE]);

void mf_plus_crypto_build_write_iv(
    const uint8_t ti[4],
    uint16_t r_ctr,
    uint16_t w_ctr,
    uint8_t iv[MF_PLUS_AES_BLOCK_SIZE]);

void mf_plus_crypto_rotate_left(
    const uint8_t input[MF_PLUS_AES_BLOCK_SIZE],
    uint8_t output[MF_PLUS_AES_BLOCK_SIZE]);

#ifdef __cplusplus
}
#endif
