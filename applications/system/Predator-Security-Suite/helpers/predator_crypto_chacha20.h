#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * ChaCha20-Poly1305 AEAD (Authenticated Encryption with Associated Data)
 * 
 * Modern encryption standard used in TLS 1.3, SSH, WireGuard, and IoT devices.
 * Faster than AES on devices without hardware acceleration.
 * 
 * Used in: Tesla charging stations, smart home devices, IoT locks, modern VPN
 * 
 * ChaCha20: Stream cipher (256-bit key, 96-bit nonce)
 * Poly1305: Message authentication code (MAC)
 * 
 * CRITICAL: For authorized security research only
 */

// Forward declare app
struct PredatorApp;

#define CHACHA20_KEY_SIZE 32        // 256-bit key
#define CHACHA20_NONCE_SIZE 12      // 96-bit nonce
#define CHACHA20_BLOCK_SIZE 64      // 512-bit block
#define POLY1305_TAG_SIZE 16        // 128-bit authentication tag

// ChaCha20 context
typedef struct {
    uint32_t state[16];             // Internal state (512 bits)
    uint8_t keystream[64];          // Current keystream block
    uint32_t keystream_pos;         // Position in keystream
    uint32_t counter;               // Block counter
} ChaCha20Context;

// Poly1305 context
typedef struct {
    uint32_t r[5];                  // R value (clamped)
    uint32_t s[4];                  // S value
    uint32_t accumulator[5];        // Running accumulator
    uint8_t buffer[16];             // Input buffer
    uint32_t buffer_len;            // Buffer length
} Poly1305Context;

// Combined AEAD context
typedef struct {
    ChaCha20Context chacha;
    Poly1305Context poly;
    uint8_t key[32];                // Original key
    uint8_t nonce[12];              // Nonce
} ChaCha20Poly1305Context;

// ========== ChaCha20 Stream Cipher ==========

/**
 * Initialize ChaCha20 context
 * @param ctx ChaCha20 context
 * @param key 256-bit key (32 bytes)
 * @param nonce 96-bit nonce (12 bytes)
 * @param counter Initial counter (usually 0)
 * @return true if successful
 */
bool chacha20_init(ChaCha20Context* ctx, const uint8_t* key, const uint8_t* nonce, uint32_t counter);

/**
 * Encrypt/decrypt data with ChaCha20 (symmetric)
 * @param ctx ChaCha20 context
 * @param data Input data
 * @param len Data length
 * @param output Output buffer (same size as input)
 * @return true if successful
 */
bool chacha20_crypt(ChaCha20Context* ctx, const uint8_t* data, size_t len, uint8_t* output);

/**
 * Generate ChaCha20 keystream
 * @param ctx ChaCha20 context
 * @param output Output buffer (64 bytes)
 * @return true if successful
 */
bool chacha20_block(ChaCha20Context* ctx, uint8_t* output);

// ========== Poly1305 MAC ==========

/**
 * Initialize Poly1305 MAC
 * @param ctx Poly1305 context
 * @param key 256-bit key (32 bytes)
 * @return true if successful
 */
bool poly1305_init(Poly1305Context* ctx, const uint8_t* key);

/**
 * Update Poly1305 with more data
 * @param ctx Poly1305 context
 * @param data Input data
 * @param len Data length
 * @return true if successful
 */
bool poly1305_update(Poly1305Context* ctx, const uint8_t* data, size_t len);

/**
 * Finalize Poly1305 and get MAC tag
 * @param ctx Poly1305 context
 * @param tag Output tag (16 bytes)
 * @return true if successful
 */
bool poly1305_finish(Poly1305Context* ctx, uint8_t* tag);

/**
 * One-shot Poly1305 MAC generation
 * @param key 256-bit key
 * @param data Input data
 * @param len Data length
 * @param tag Output tag (16 bytes)
 * @return true if successful
 */
bool poly1305_mac(const uint8_t* key, const uint8_t* data, size_t len, uint8_t* tag);

// ========== ChaCha20-Poly1305 AEAD ==========

/**
 * Initialize ChaCha20-Poly1305 AEAD context
 * @param ctx AEAD context
 * @param key 256-bit key (32 bytes)
 * @param nonce 96-bit nonce (12 bytes)
 * @return true if successful
 */
bool chacha20_poly1305_init(ChaCha20Poly1305Context* ctx, const uint8_t* key, const uint8_t* nonce);

/**
 * Encrypt and authenticate (AEAD)
 * @param ctx AEAD context
 * @param plaintext Plaintext data
 * @param plaintext_len Plaintext length
 * @param aad Additional authenticated data (can be NULL)
 * @param aad_len AAD length
 * @param ciphertext Output ciphertext (same size as plaintext)
 * @param tag Output authentication tag (16 bytes)
 * @return true if successful
 */
bool chacha20_poly1305_encrypt(ChaCha20Poly1305Context* ctx,
                               const uint8_t* plaintext, size_t plaintext_len,
                               const uint8_t* aad, size_t aad_len,
                               uint8_t* ciphertext, uint8_t* tag);

/**
 * Decrypt and verify (AEAD)
 * @param ctx AEAD context
 * @param ciphertext Ciphertext data
 * @param ciphertext_len Ciphertext length
 * @param aad Additional authenticated data (can be NULL)
 * @param aad_len AAD length
 * @param tag Authentication tag (16 bytes)
 * @param plaintext Output plaintext (same size as ciphertext)
 * @return true if successful and authenticated
 */
bool chacha20_poly1305_decrypt(ChaCha20Poly1305Context* ctx,
                               const uint8_t* ciphertext, size_t ciphertext_len,
                               const uint8_t* aad, size_t aad_len,
                               const uint8_t* tag, uint8_t* plaintext);

// ========== High-Level Wrapper Functions ==========

/**
 * Simple encrypt with ChaCha20-Poly1305
 * @param key 256-bit key
 * @param nonce 96-bit nonce (must be unique for each message with same key!)
 * @param plaintext Plaintext data
 * @param plaintext_len Plaintext length
 * @param ciphertext Output ciphertext
 * @param tag Output authentication tag (16 bytes)
 * @return true if successful
 */
bool predator_crypto_chacha20_poly1305_encrypt(const uint8_t* key, const uint8_t* nonce,
                                                const uint8_t* plaintext, size_t plaintext_len,
                                                uint8_t* ciphertext, uint8_t* tag);

/**
 * Simple decrypt with ChaCha20-Poly1305
 * @param key 256-bit key
 * @param nonce 96-bit nonce
 * @param ciphertext Ciphertext data
 * @param ciphertext_len Ciphertext length
 * @param tag Authentication tag (16 bytes)
 * @param plaintext Output plaintext
 * @return true if successful and authenticated
 */
bool predator_crypto_chacha20_poly1305_decrypt(const uint8_t* key, const uint8_t* nonce,
                                                const uint8_t* ciphertext, size_t ciphertext_len,
                                                const uint8_t* tag, uint8_t* plaintext);

// ========== IoT-Specific Functions ==========

/**
 * Tesla charging station authentication
 * @param app PredatorApp context
 * @param station_key Station key (32 bytes)
 * @param challenge Challenge data
 * @param challenge_len Challenge length
 * @param response Output response
 * @param response_len Output response length
 * @return true if successful
 */
bool chacha20_tesla_station_auth(struct PredatorApp* app,
                                 const uint8_t* station_key,
                                 const uint8_t* challenge, size_t challenge_len,
                                 uint8_t* response, size_t* response_len);

/**
 * Smart lock encrypted command
 * @param app PredatorApp context
 * @param lock_key Lock key (32 bytes)
 * @param command Command to send
 * @param command_len Command length
 * @param encrypted_out Output encrypted command
 * @param tag_out Output authentication tag
 * @return true if successful
 */
bool chacha20_smart_lock_command(struct PredatorApp* app,
                                 const uint8_t* lock_key,
                                 const uint8_t* command, size_t command_len,
                                 uint8_t* encrypted_out, uint8_t* tag_out);

/**
 * IoT device secure communication
 * @param device_key Device key (32 bytes)
 * @param nonce Nonce (12 bytes)
 * @param message Message to encrypt
 * @param message_len Message length
 * @param encrypted_out Output encrypted message
 * @param tag_out Output authentication tag
 * @return true if successful
 */
bool chacha20_iot_encrypt(const uint8_t* device_key, const uint8_t* nonce,
                          const uint8_t* message, size_t message_len,
                          uint8_t* encrypted_out, uint8_t* tag_out);

// ========== Utilities ==========

/**
 * Generate random nonce
 * @param nonce Output nonce buffer (12 bytes)
 */
void chacha20_generate_nonce(uint8_t* nonce);

/**
 * Constant-time memory comparison (for tag verification)
 * @param a First buffer
 * @param b Second buffer
 * @param len Length to compare
 * @return true if equal
 */
bool chacha20_constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len);

/**
 * Key derivation from password (simplified PBKDF2)
 * @param password Password string
 * @param salt Salt (8+ bytes recommended)
 * @param salt_len Salt length
 * @param iterations Number of iterations (10000+ recommended)
 * @param key Output key (32 bytes)
 * @return true if successful
 */
bool chacha20_derive_key(const char* password, const uint8_t* salt, size_t salt_len,
                         uint32_t iterations, uint8_t* key);

// ========== Test Vectors (RFC 8439) ==========

/**
 * Run ChaCha20-Poly1305 test vectors
 * @return true if all tests pass
 */
bool chacha20_poly1305_self_test(void);

/**
 * AUTHORIZATION WARNING:
 * 
 * ChaCha20-Poly1305 tools must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized IoT device access
 * - Breaking encrypted communications without authorization
 * - Smart lock attacks without authorization
 * - Charging station fraud or unauthorized access
 * - Any form of unauthorized system access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own IoT devices
 * - Authorized penetration testing with contracts
 * - Security research in controlled environments
 * - Educational purposes with proper oversight
 * - Vulnerability research with responsible disclosure
 * 
 * Unauthorized cryptographic attacks are ILLEGAL in most jurisdictions.
 */
