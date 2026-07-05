#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * HID iClass (Picopass) Implementation
 * 
 * For authorized physical access control security research.
 * Used in corporate offices, hospitals, universities, government facilities.
 * Very common in North American enterprise environments.
 * 
 * CRITICAL: Obtain written authorization before testing any access control systems.
 */

// Forward declare app
struct PredatorApp;

// iClass card types
typedef enum {
    iClassStandard,         // 2KB (2 application areas)
    iClassStandard2K,       // 2KB with enhanced features
    iClass16K,              // 16KB (16 application areas)
    iClass32K,              // 32KB (32 application areas)
    iClassSE,               // Secure Element (latest, AES)
    iClassSeos              // SEOS (mobile credential)
} iClassCardType;

// Security features
typedef enum {
    iClassSecurityLegacy,   // DES encryption (broken)
    iClassSecurityElite,    // 3DES encryption
    iClassSecuritySR,       // Secure Read
    iClassSecuritySIO       // Secure Identity Object
} iClassSecurityType;

// Key structure
typedef struct {
    uint8_t key[8];         // DES/3DES key (8 bytes)
    bool is_debit_key;      // true = debit key, false = credit key
    uint8_t block_number;   // Associated block
} iClassKey;

// Card structure
typedef struct {
    iClassCardType type;
    iClassSecurityType security;
    uint8_t csn[8];         // Card Serial Number
    uint8_t config[8];      // Configuration block
    uint8_t e_purse[8];     // E-purse data
    iClassKey keys[16];     // Application keys
    uint8_t keys_found;
    bool authenticated;
} iClassCard;

// Attack state
typedef struct {
    iClassCard card;
    uint64_t keys_tried;
    uint32_t start_time;
    bool attack_running;
    uint8_t current_block;
} iClassAttackState;

// ========== iClass Crypto Functions ==========

/**
 * DES encryption (legacy iClass)
 * @param key 8-byte DES key
 * @param plaintext 8-byte input
 * @param ciphertext 8-byte output
 * @return true if successful
 */
bool iclass_des_encrypt(const uint8_t* key,
                        const uint8_t* plaintext,
                        uint8_t* ciphertext);

/**
 * DES decryption
 * @param key 8-byte DES key
 * @param ciphertext 8-byte input
 * @param plaintext 8-byte output
 * @return true if successful
 */
bool iclass_des_decrypt(const uint8_t* key,
                        const uint8_t* ciphertext,
                        uint8_t* plaintext);

/**
 * Generate diversified key
 * @param master_key Master application key
 * @param csn Card Serial Number
 * @param diversified_key Output key
 * @return true if successful
 */
bool iclass_diversify_key(const uint8_t* master_key,
                          const uint8_t* csn,
                          uint8_t* diversified_key);

/**
 * Calculate MAC (Message Authentication Code)
 * @param key Encryption key
 * @param data Data to authenticate
 * @param len Data length
 * @param mac Output MAC
 * @return true if successful
 */
bool iclass_calculate_mac(const uint8_t* key,
                          const uint8_t* data,
                          uint32_t len,
                          uint8_t* mac);

// ========== Card Operations ==========

/**
 * Detect iClass card
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if card detected
 */
bool iclass_detect_card(struct PredatorApp* app, iClassCard* card);

/**
 * Read CSN (Card Serial Number)
 * @param app PredatorApp context
 * @param csn Output CSN buffer (8 bytes)
 * @return true if successful
 */
bool iclass_read_csn(struct PredatorApp* app, uint8_t* csn);

/**
 * Authenticate to card
 * @param app PredatorApp context
 * @param key Key to use
 * @return true if authenticated
 */
bool iclass_authenticate(struct PredatorApp* app, const iClassKey* key);

/**
 * Read block
 * @param app PredatorApp context
 * @param block Block number (0-255)
 * @param data Output buffer (8 bytes)
 * @return true if successful
 */
bool iclass_read_block(struct PredatorApp* app, uint8_t block, uint8_t* data);

/**
 * Write block
 * @param app PredatorApp context
 * @param block Block number
 * @param data Data to write (8 bytes)
 * @return true if successful
 */
bool iclass_write_block(struct PredatorApp* app, uint8_t block, const uint8_t* data);

// ========== Attack Functions ==========

/**
 * Dictionary attack - try known keys
 * @param app PredatorApp context
 * @param card Target card
 * @return Number of keys found
 */
uint32_t iclass_attack_dictionary(struct PredatorApp* app, iClassCard* card);

/**
 * Downgrade attack - force legacy mode
 * @param app PredatorApp context
 * @param card Target card
 * @return true if successful
 */
bool iclass_attack_downgrade(struct PredatorApp* app, iClassCard* card);

/**
 * Loclass attack - optimized key recovery
 * @param app PredatorApp context
 * @param card Target card
 * @param found_key Output key
 * @return true if key found
 */
bool iclass_attack_loclass(struct PredatorApp* app, 
                           iClassCard* card,
                           iClassKey* found_key);

/**
 * Clone card
 * @param app PredatorApp context
 * @param source Source card
 * @param target Target card (writeable)
 * @return true if cloned
 */
bool iclass_clone_card(struct PredatorApp* app,
                       const iClassCard* source,
                       iClassCard* target);

// ========== Key Management ==========

/**
 * Load default keys
 * @param keys Output key array
 * @param max_keys Maximum keys
 * @return Number of keys loaded
 */
uint32_t iclass_load_default_keys(iClassKey* keys, uint32_t max_keys);

/**
 * Load HID master keys (if known)
 * @param keys Output key array
 * @param max_keys Maximum keys
 * @return Number of keys loaded
 */
uint32_t iclass_load_master_keys(iClassKey* keys, uint32_t max_keys);

/**
 * Validate key
 * @param key Key to validate
 * @return true if valid
 */
bool iclass_validate_key(const iClassKey* key);

/**
 * Key to string
 * @param key Key structure
 * @param str Output string buffer
 */
void iclass_key_to_string(const iClassKey* key, char* str);

// ========== Facility Code Operations ==========

/**
 * Read facility code from card
 * @param card iClass card
 * @return Facility code (0 if not found)
 */
uint16_t iclass_read_facility_code(const iClassCard* card);

/**
 * Read card number
 * @param card iClass card
 * @return Card number (0 if not found)
 */
uint32_t iclass_read_card_number(const iClassCard* card);

/**
 * Decode Wiegand data
 * @param data Raw data
 * @param facility_code Output facility code
 * @param card_number Output card number
 * @return true if decoded
 */
bool iclass_decode_wiegand(const uint8_t* data,
                           uint16_t* facility_code,
                           uint32_t* card_number);

// ========== Utilities ==========

/**
 * Calculate CRC for iClass
 * @param data Data buffer
 * @param len Data length
 * @return CRC value
 */
uint16_t iclass_crc(const uint8_t* data, uint32_t len);

/**
 * Get block count for card type
 * @param type Card type
 * @return Number of blocks
 */
uint32_t iclass_get_block_count(iClassCardType type);

/**
 * Detect card type from CSN
 * @param csn Card Serial Number
 * @return Card type
 */
iClassCardType iclass_detect_type(const uint8_t* csn);

// ========== Default Keys ==========

// HID default keys (publicly known)
extern const uint8_t ICLASS_KEY_DEFAULT[8];
extern const uint8_t ICLASS_KEY_DEBIT[8];
extern const uint8_t ICLASS_KEY_CREDIT[8];

// Corporate default keys (common but should be changed)
extern const uint8_t ICLASS_KEY_CORPORATE_1[8];
extern const uint8_t ICLASS_KEY_CORPORATE_2[8];

// Healthcare default keys
extern const uint8_t ICLASS_KEY_HOSPITAL_1[8];
extern const uint8_t ICLASS_KEY_HOSPITAL_2[8];

// University default keys
extern const uint8_t ICLASS_KEY_UNIVERSITY_1[8];
extern const uint8_t ICLASS_KEY_UNIVERSITY_2[8];

/**
 * AUTHORIZATION WARNING:
 * 
 * HID iClass attacks must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized building or office access
 * - Badge cloning without authorization
 * - Trespassing or unauthorized entry
 * - Identity theft or impersonation
 * - Any form of corporate espionage
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own facility's access control
 * - Authorized corporate security audits
 * - Professional penetration testing with contracts
 * - Educational research in controlled environments
 * - Vulnerability research with responsible disclosure
 * 
 * Unauthorized access control bypass is a FELONY in most jurisdictions.
 * May violate computer fraud, trespass, burglary, and impersonation laws.
 */
