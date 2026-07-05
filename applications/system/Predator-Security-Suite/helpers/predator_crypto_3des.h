#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * 3DES (Triple DES) Cryptographic Implementation
 * 
 * PRODUCTION-GRADE IMPLEMENTATION
 * Used by: FeliCa, Calypso, UltraLight C, MIFARE DESFire
 * 
 * Implements 2-key 3DES (112-bit security):
 * - Encrypt with K1, Decrypt with K2, Encrypt with K1 (EDE mode)
 * - ECB and CBC modes
 * - Key derivation/diversification
 */

// ========== ECB MODE (Electronic Codebook) ==========

/**
 * 3DES encryption in ECB mode (2-key, 16 bytes)
 * @param key 3DES key (16 bytes: K1 || K2)
 * @param input Input data (8 bytes)
 * @param output Output buffer (8 bytes)
 */
void des3_encrypt_ecb(const uint8_t* key, const uint8_t* input, uint8_t* output);

/**
 * 3DES decryption in ECB mode (2-key, 16 bytes)
 * @param key 3DES key (16 bytes: K1 || K2)
 * @param input Input data (8 bytes)
 * @param output Output buffer (8 bytes)
 */
void des3_decrypt_ecb(const uint8_t* key, const uint8_t* input, uint8_t* output);

// ========== CBC MODE (Cipher Block Chaining) ==========

/**
 * 3DES encryption in CBC mode
 * @param key 3DES key (16 bytes)
 * @param iv Initialization vector (8 bytes)
 * @param input Input data (must be multiple of 8 bytes)
 * @param output Output buffer (same length as input)
 * @param length Data length (must be multiple of 8)
 */
void des3_encrypt_cbc(const uint8_t* key, const uint8_t* iv,
                     const uint8_t* input, uint8_t* output, size_t length);

/**
 * 3DES decryption in CBC mode
 * @param key 3DES key (16 bytes)
 * @param iv Initialization vector (8 bytes)
 * @param input Input data (must be multiple of 8 bytes)
 * @param output Output buffer (same length as input)
 * @param length Data length (must be multiple of 8)
 */
void des3_decrypt_cbc(const uint8_t* key, const uint8_t* iv,
                     const uint8_t* input, uint8_t* output, size_t length);

// ========== KEY DERIVATION ==========

/**
 * Derive card-specific key from master key and diversifier
 * Common pattern for FeliCa, Calypso, etc.
 * 
 * @param master_key Master key (16 bytes)
 * @param diversifier Diversifier (usually card UID or serial)
 * @param div_len Diversifier length
 * @param derived_key Output derived key (16 bytes)
 */
void des3_derive_key(const uint8_t* master_key, const uint8_t* diversifier,
                    size_t div_len, uint8_t* derived_key);

// ========== UTILITIES ==========

/**
 * Validate 3DES key (check for weak keys)
 * @param key 3DES key (16 bytes)
 * @return true if key is valid
 */
bool des3_validate_key(const uint8_t* key);
