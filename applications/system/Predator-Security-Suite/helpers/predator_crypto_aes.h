#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Full AES-128 and AES-256 Implementation
 * 
 * Production-grade Advanced Encryption Standard implementation
 * Used in: Tesla, BMW, Mercedes, modern smart keys, IoT devices
 * 
 * AES-128: 10 rounds, 128-bit key
 * AES-256: 14 rounds, 256-bit key
 * 
 * Compliant with FIPS-197 standard
 */

// Forward declare app
struct PredatorApp;

// AES block size (always 128-bit = 16 bytes)
#define AES_BLOCK_SIZE 16
#define AES128_KEY_SIZE 16
#define AES256_KEY_SIZE 32
#define AES128_ROUNDS 10
#define AES256_ROUNDS 14

// AES context for encryption/decryption
typedef struct {
    uint8_t round_keys[240];  // Expanded key schedule (max 15 rounds * 16 bytes)
    uint8_t num_rounds;       // 10 for AES-128, 14 for AES-256
    bool is_aes256;           // Key size flag
} AESContext;

// ========== Core AES Functions ==========

/**
 * Initialize AES-128 context with key expansion
 * @param ctx AES context to initialize
 * @param key 128-bit key (16 bytes)
 * @return true if successful
 */
bool aes128_init(AESContext* ctx, const uint8_t* key);

/**
 * Initialize AES-256 context with key expansion
 * @param ctx AES context to initialize
 * @param key 256-bit key (32 bytes)
 * @return true if successful
 */
bool aes256_init(AESContext* ctx, const uint8_t* key);

/**
 * Encrypt single block with AES-128 (full 10 rounds)
 * @param ctx Initialized AES-128 context
 * @param plaintext 16-byte input block
 * @param ciphertext 16-byte output block
 * @return true if successful
 */
bool aes128_encrypt_block(const AESContext* ctx, const uint8_t* plaintext, uint8_t* ciphertext);

/**
 * Decrypt single block with AES-128 (full 10 rounds)
 * @param ctx Initialized AES-128 context
 * @param ciphertext 16-byte input block
 * @param plaintext 16-byte output block
 * @return true if successful
 */
bool aes128_decrypt_block(const AESContext* ctx, const uint8_t* ciphertext, uint8_t* plaintext);

/**
 * Encrypt single block with AES-256 (full 14 rounds)
 * @param ctx Initialized AES-256 context
 * @param plaintext 16-byte input block
 * @param ciphertext 16-byte output block
 * @return true if successful
 */
bool aes256_encrypt_block(const AESContext* ctx, const uint8_t* plaintext, uint8_t* ciphertext);

/**
 * Decrypt single block with AES-256 (full 14 rounds)
 * @param ctx Initialized AES-256 context
 * @param ciphertext 16-byte input block
 * @param plaintext 16-byte output block
 * @return true if successful
 */
bool aes256_decrypt_block(const AESContext* ctx, const uint8_t* ciphertext, uint8_t* plaintext);

// ========== High-Level Encryption Functions ==========

/**
 * AES-128 ECB mode encryption (multiple blocks)
 * @param key 16-byte key
 * @param data Input data (must be multiple of 16 bytes)
 * @param len Data length (must be multiple of 16)
 * @param output Output buffer (same size as data)
 * @return true if successful
 */
bool predator_crypto_aes128_encrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output);

/**
 * AES-128 ECB mode decryption (multiple blocks)
 * @param key 16-byte key
 * @param data Input data (must be multiple of 16 bytes)
 * @param len Data length (must be multiple of 16)
 * @param output Output buffer (same size as data)
 * @return true if successful
 */
bool predator_crypto_aes128_decrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output);

/**
 * AES-256 ECB mode encryption (multiple blocks)
 * @param key 32-byte key
 * @param data Input data (must be multiple of 16 bytes)
 * @param len Data length (must be multiple of 16)
 * @param output Output buffer (same size as data)
 * @return true if successful
 */
bool predator_crypto_aes256_encrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output);

/**
 * AES-256 ECB mode decryption (multiple blocks)
 * @param key 32-byte key
 * @param data Input data (must be multiple of 16 bytes)
 * @param len Data length (must be multiple of 16)
 * @param output Output buffer (same size as data)
 * @return true if successful
 */
bool predator_crypto_aes256_decrypt(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* output);

// ========== AES-CBC Mode (Cipher Block Chaining) ==========

/**
 * AES-128 CBC encryption (more secure than ECB)
 * @param key 16-byte key
 * @param iv 16-byte initialization vector
 * @param data Input data (must be multiple of 16 bytes)
 * @param len Data length
 * @param output Output buffer
 * @return true if successful
 */
bool predator_crypto_aes128_cbc_encrypt(const uint8_t* key, const uint8_t* iv, 
                                        const uint8_t* data, size_t len, uint8_t* output);

/**
 * AES-128 CBC decryption
 * @param key 16-byte key
 * @param iv 16-byte initialization vector
 * @param data Input data
 * @param len Data length
 * @param output Output buffer
 * @return true if successful
 */
bool predator_crypto_aes128_cbc_decrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output);

/**
 * AES-256 CBC encryption
 * @param key 32-byte key
 * @param iv 16-byte initialization vector
 * @param data Input data
 * @param len Data length
 * @param output Output buffer
 * @return true if successful
 */
bool predator_crypto_aes256_cbc_encrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output);

/**
 * AES-256 CBC decryption
 * @param key 32-byte key
 * @param iv 16-byte initialization vector
 * @param data Input data
 * @param len Data length
 * @param output Output buffer
 * @return true if successful
 */
bool predator_crypto_aes256_cbc_decrypt(const uint8_t* key, const uint8_t* iv,
                                        const uint8_t* data, size_t len, uint8_t* output);

// ========== Automotive-Specific AES Functions ==========

/**
 * Tesla smart key challenge-response (AES-256)
 * @param app PredatorApp context
 * @param vehicle_key 32-byte Tesla vehicle key
 * @param challenge 16-byte random challenge
 * @param response 16-byte output response
 * @return true if successful
 */
bool aes_tesla_challenge_response(struct PredatorApp* app, 
                                  const uint8_t* vehicle_key,
                                  const uint8_t* challenge,
                                  uint8_t* response);

/**
 * BMW smart key authentication (AES-128)
 * @param app PredatorApp context
 * @param key 16-byte BMW key
 * @param vehicle_id 8-byte vehicle ID
 * @param auth_token 16-byte output token
 * @return true if successful
 */
bool aes_bmw_authenticate(struct PredatorApp* app,
                         const uint8_t* key,
                         const uint8_t* vehicle_id,
                         uint8_t* auth_token);

/**
 * Mercedes encrypted packet (AES-128)
 * @param key 16-byte Mercedes key
 * @param packet_data Raw packet data
 * @param len Packet length
 * @param encrypted_out Output buffer
 * @return true if successful
 */
bool aes_mercedes_encrypt_packet(const uint8_t* key,
                                 const uint8_t* packet_data,
                                 size_t len,
                                 uint8_t* encrypted_out);

// ========== Utilities ==========

/**
 * PKCS#7 padding for AES (pad to 16-byte boundary)
 * @param data Input data
 * @param len Data length
 * @param padded_out Output buffer (must have space for padding)
 * @param padded_len Output padded length
 * @return true if successful
 */
bool aes_pkcs7_pad(const uint8_t* data, size_t len, uint8_t* padded_out, size_t* padded_len);

/**
 * Remove PKCS#7 padding
 * @param padded Padded data
 * @param padded_len Padded length
 * @param unpadded_len Output original length
 * @return true if valid padding
 */
bool aes_pkcs7_unpad(const uint8_t* padded, size_t padded_len, size_t* unpadded_len);

/**
 * Generate random IV (Initialization Vector)
 * @param iv Output 16-byte IV
 */
void aes_generate_iv(uint8_t* iv);

/**
 * AUTHORIZATION WARNING:
 * 
 * AES encryption tools must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized vehicle access or theft
 * - Breaking encryption without authorization
 * - Any form of unauthorized system access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own vehicle security
 * - Authorized automotive security research
 * - Professional penetration testing with contracts
 * - Educational research in controlled environments
 * 
 * Unauthorized cryptographic attacks are ILLEGAL in most jurisdictions.
 */
