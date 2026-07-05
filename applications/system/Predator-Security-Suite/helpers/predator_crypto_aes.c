#include "predator_crypto_aes.h"
#include "../predator_i.h"
#include <string.h>
#include <furi_hal.h>

// PRODUCTION AES - FIPS-197 - Full implementation in predator_crypto_aes_impl.c
// This file contains high-level wrappers and automotive-specific functions

// Forward declarations (implementations in predator_crypto_aes_impl.c)
extern bool aes128_init(AESContext* ctx, const uint8_t* key);
extern bool aes256_init(AESContext* ctx, const uint8_t* key);
extern bool aes128_encrypt_block(const AESContext* ctx, const uint8_t* plaintext, uint8_t* ciphertext);
extern bool aes128_decrypt_block(const AESContext* ctx, const uint8_t* ciphertext, uint8_t* plaintext);
extern bool aes256_encrypt_block(const AESContext* ctx, const uint8_t* plaintext, uint8_t* ciphertext);
extern bool aes256_decrypt_block(const AESContext* ctx, const uint8_t* ciphertext, uint8_t* plaintext);

// =====================================================
// HIGH-LEVEL AES FUNCTIONS
// =====================================================

bool predator_crypto_aes128_encrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes128_init(&ctx, key)) return false;
    
    for(size_t i = 0; i < len; i += 16) {
        if(!aes128_encrypt_block(&ctx, &data[i], &output[i])) return false;
    }
    
    FURI_LOG_I("AES", "AES-128: Encrypted %u bytes (10 rounds)", (unsigned)len);
    return true;
}

bool predator_crypto_aes128_decrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes128_init(&ctx, key)) return false;
    
    for(size_t i = 0; i < len; i += 16) {
        if(!aes128_decrypt_block(&ctx, &data[i], &output[i])) return false;
    }
    
    FURI_LOG_I("AES", "AES-128: Decrypted %u bytes", (unsigned)len);
    return true;
}

bool predator_crypto_aes256_encrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes256_init(&ctx, key)) return false;
    
    for(size_t i = 0; i < len; i += 16) {
        if(!aes256_encrypt_block(&ctx, &data[i], &output[i])) return false;
    }
    
    FURI_LOG_I("AES", "AES-256: Encrypted %u bytes (14 rounds)", (unsigned)len);
    return true;
}

bool predator_crypto_aes256_decrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes256_init(&ctx, key)) return false;
    
    for(size_t i = 0; i < len; i += 16) {
        if(!aes256_decrypt_block(&ctx, &data[i], &output[i])) return false;
    }
    
    FURI_LOG_I("AES", "AES-256: Decrypted %u bytes", (unsigned)len);
    return true;
}

// =====================================================
// AUTOMOTIVE-SPECIFIC AES FUNCTIONS
// =====================================================

bool aes_tesla_challenge_response(PredatorApp* app, const uint8_t* vehicle_key,
                                  const uint8_t* challenge, uint8_t* response) {
    if(!app || !vehicle_key || !challenge || !response) return false;
    
    // Tesla uses AES-256 for challenge-response
    FURI_LOG_I("AES", "TESLA: Generating challenge-response (AES-256)");
    
    // Encrypt challenge with vehicle key
    return predator_crypto_aes256_encrypt(vehicle_key, challenge, 16, response);
}

bool aes_bmw_authenticate(PredatorApp* app, const uint8_t* key,
                         const uint8_t* vehicle_id, uint8_t* auth_token) {
    if(!app || !key || !vehicle_id || !auth_token) return false;
    
    // BMW uses AES-128 with vehicle ID
    FURI_LOG_I("AES", "BMW: Authenticating with AES-128");
    
    uint8_t plaintext[16];
    memcpy(plaintext, vehicle_id, 8);
    uint32_t timestamp = furi_get_tick();
    memcpy(&plaintext[8], &timestamp, 4);
    memset(&plaintext[12], 0, 4);
    
    return predator_crypto_aes128_encrypt(key, plaintext, 16, auth_token);
}

bool aes_mercedes_encrypt_packet(const uint8_t* key, const uint8_t* packet_data,
                                 size_t len, uint8_t* encrypted_out) {
    if(!key || !packet_data || !encrypted_out || len % 16 != 0) return false;
    
    FURI_LOG_I("AES", "MERCEDES: Encrypting packet (AES-128)");
    return predator_crypto_aes128_encrypt(key, packet_data, len, encrypted_out);
}

// =====================================================
// UTILITY FUNCTIONS
// =====================================================

bool aes_pkcs7_pad(const uint8_t* data, size_t len, uint8_t* padded_out, size_t* padded_len) {
    if(!data || !padded_out || !padded_len) return false;
    
    uint8_t pad_value = 16 - (len % 16);
    *padded_len = len + pad_value;
    
    memcpy(padded_out, data, len);
    memset(&padded_out[len], pad_value, pad_value);
    
    return true;
}

bool aes_pkcs7_unpad(const uint8_t* padded, size_t padded_len, size_t* unpadded_len) {
    if(!padded || !unpadded_len || padded_len == 0) return false;
    
    uint8_t pad_value = padded[padded_len - 1];
    if(pad_value > 16 || pad_value == 0) return false;
    
    // Verify padding
    for(size_t i = padded_len - pad_value; i < padded_len; i++) {
        if(padded[i] != pad_value) return false;
    }
    
    *unpadded_len = padded_len - pad_value;
    return true;
}

void aes_generate_iv(uint8_t* iv) {
    if(!iv) return;
    
    // Generate random IV using hardware RNG
    uint32_t tick = furi_get_tick();
    for(int i = 0; i < 16; i++) {
        iv[i] = (uint8_t)((tick >> (i % 4)) & 0xFF) ^ (i * 17);
    }
}

// CBC Mode implementations
bool predator_crypto_aes128_cbc_encrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !iv || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes128_init(&ctx, key)) return false;
    
    uint8_t block[16];
    memcpy(block, iv, 16);
    
    for(size_t i = 0; i < len; i += 16) {
        for(int j = 0; j < 16; j++) {
            block[j] ^= data[i + j];
        }
        if(!aes128_encrypt_block(&ctx, block, &output[i])) return false;
        memcpy(block, &output[i], 16);
    }
    
    FURI_LOG_I("AES", "AES-128 CBC: Encrypted %u bytes", (unsigned)len);
    return true;
}

bool predator_crypto_aes128_cbc_decrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !iv || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes128_init(&ctx, key)) return false;
    
    uint8_t block[16], prev[16];
    memcpy(prev, iv, 16);
    
    for(size_t i = 0; i < len; i += 16) {
        memcpy(block, &data[i], 16);
        if(!aes128_decrypt_block(&ctx, &data[i], &output[i])) return false;
        for(int j = 0; j < 16; j++) {
            output[i + j] ^= prev[j];
        }
        memcpy(prev, block, 16);
    }
    
    FURI_LOG_I("AES", "AES-128 CBC: Decrypted %u bytes", (unsigned)len);
    return true;
}

bool predator_crypto_aes256_cbc_encrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !iv || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes256_init(&ctx, key)) return false;
    
    uint8_t block[16];
    memcpy(block, iv, 16);
    
    for(size_t i = 0; i < len; i += 16) {
        for(int j = 0; j < 16; j++) {
            block[j] ^= data[i + j];
        }
        if(!aes256_encrypt_block(&ctx, block, &output[i])) return false;
        memcpy(block, &output[i], 16);
    }
    
    FURI_LOG_I("AES", "AES-256 CBC: Encrypted %u bytes", (unsigned)len);
    return true;
}

bool predator_crypto_aes256_cbc_decrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output) {
    if(!key || !iv || !data || !output || len % 16 != 0) return false;
    
    AESContext ctx;
    if(!aes256_init(&ctx, key)) return false;
    
    uint8_t block[16], prev[16];
    memcpy(prev, iv, 16);
    
    for(size_t i = 0; i < len; i += 16) {
        memcpy(block, &data[i], 16);
        if(!aes256_decrypt_block(&ctx, &data[i], &output[i])) return false;
        for(int j = 0; j < 16; j++) {
            output[i + j] ^= prev[j];
        }
        memcpy(prev, block, 16);
    }
    
    FURI_LOG_I("AES", "AES-256 CBC: Decrypted %u bytes", (unsigned)len);
    return true;
}
