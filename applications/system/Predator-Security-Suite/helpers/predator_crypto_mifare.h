#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * MIFARE Classic Crypto1 Implementation
 * 
 * For authorized security research and physical access control testing.
 * Used in 90%+ of hotel key cards, building access, parking systems.
 * 
 * CRITICAL: Obtain written authorization before testing any RFID systems.
 */

// Forward declare app
struct PredatorApp;

// MIFARE Classic card types
typedef enum {
    MifareClassic1K,   // 1KB (16 sectors, 4 blocks each)
    MifareClassic4K,   // 4KB (40 sectors)
    MifareClassicMini  // 320 bytes (5 sectors)
} MifareCardType;

// Attack types
typedef enum {
    MifareAttackDictionary,    // Try known keys
    MifareAttackNested,        // Nested authentication attack
    MifareAttackDarkside,      // Darkside attack (one known key)
    MifareAttackHardnested,    // Advanced nested (no known keys)
    MifareAttackStaticNested   // Static encrypted nonce
} MifareAttackType;

// Key structure
typedef struct {
    uint8_t key[6];            // 48-bit key
    uint8_t sector;            // Sector number
    bool is_key_a;             // true = Key A, false = Key B
} MifareKey;

// Card data structure
typedef struct {
    MifareCardType type;
    uint8_t uid[4];            // 4-byte UID (Classic)
    uint8_t uid_len;
    uint8_t sak;               // Select Acknowledge
    uint8_t atqa[2];           // Answer to Request
    MifareKey keys[40];        // Max 40 sectors (4K card)
    uint8_t keys_found;
    bool keys_valid[40];
} MifareCard;

// Attack state
typedef struct {
    MifareAttackType attack_type;
    MifareCard* card;
    uint32_t keys_tried;
    uint32_t keys_found;
    uint8_t current_sector;
    bool attack_running;
    uint32_t start_time;
} MifareAttackState;

// ========== Crypto1 Cipher Functions ==========

/**
 * Initialize Crypto1 cipher with key
 * @param key 48-bit key (6 bytes)
 * @return Crypto1 state (64-bit)
 */
uint64_t crypto1_init(const uint8_t* key);

/**
 * Crypto1 LFSR feedback function
 * @param state Current cipher state
 * @param in Input bit
 * @return Output bit
 */
uint8_t crypto1_bit(uint64_t* state, uint8_t in, uint8_t is_encrypted);

/**
 * Crypto1 byte operation
 * @param state Current cipher state
 * @param in Input byte
 * @param is_encrypted Encryption flag
 * @return Output byte
 */
uint8_t crypto1_byte(uint64_t* state, uint8_t in, uint8_t is_encrypted);

/**
 * Generate authentication response
 * @param uid Card UID
 * @param nonce Challenge nonce
 * @param key 48-bit key
 * @return Authentication tag
 */
uint32_t crypto1_auth(const uint8_t* uid, uint32_t nonce, const uint8_t* key);

// ========== MIFARE Operations ==========

/**
 * Detect MIFARE Classic card
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if card detected
 */
bool mifare_detect_card(struct PredatorApp* app, MifareCard* card);

/**
 * Authenticate to sector
 * @param app PredatorApp context
 * @param sector Sector number (0-39)
 * @param key Key to use
 * @return true if authentication successful
 */
bool mifare_authenticate(struct PredatorApp* app, uint8_t sector, const MifareKey* key);

/**
 * Read block data
 * @param app PredatorApp context
 * @param block Block number (0-255)
 * @param data Output buffer (16 bytes)
 * @return true if read successful
 */
bool mifare_read_block(struct PredatorApp* app, uint8_t block, uint8_t* data);

/**
 * Write block data
 * @param app PredatorApp context
 * @param block Block number
 * @param data Data to write (16 bytes)
 * @return true if write successful
 */
bool mifare_write_block(struct PredatorApp* app, uint8_t block, const uint8_t* data);

// ========== Attack Functions ==========

/**
 * Dictionary attack - try known keys
 * @param app PredatorApp context
 * @param card Target card
 * @return Number of keys found
 */
uint32_t mifare_attack_dictionary(struct PredatorApp* app, MifareCard* card);

/**
 * Nested attack - exploit authentication weakness
 * @param app PredatorApp context
 * @param card Target card
 * @param known_key One known key (any sector)
 * @return Number of keys found
 */
uint32_t mifare_attack_nested(struct PredatorApp* app, MifareCard* card, const MifareKey* known_key);

/**
 * Darkside attack - no known keys required
 * @param app PredatorApp context
 * @param card Target card
 * @return true if successful
 */
bool mifare_attack_darkside(struct PredatorApp* app, MifareCard* card);

/**
 * Hardnested attack - advanced nested
 * @param app PredatorApp context
 * @param card Target card
 * @return Number of keys found
 */
uint32_t mifare_attack_hardnested(struct PredatorApp* app, MifareCard* card);

// ========== Key Management ==========

/**
 * Load default keys dictionary
 * @param keys Output key array
 * @param max_keys Maximum keys to load
 * @return Number of keys loaded
 */
uint32_t mifare_load_default_keys(MifareKey* keys, uint32_t max_keys);

/**
 * Check if key is valid
 * @param key Key to validate
 * @return true if valid
 */
bool mifare_validate_key(const MifareKey* key);

/**
 * Convert key to string
 * @param key Key structure
 * @param str Output string buffer (18 bytes min)
 */
void mifare_key_to_string(const MifareKey* key, char* str);

// ========== Utilities ==========

/**
 * Get sector count for card type
 * @param type Card type
 * @return Number of sectors
 */
uint8_t mifare_get_sector_count(MifareCardType type);

/**
 * Get block count for sector
 * @param sector Sector number
 * @param type Card type
 * @return Number of blocks
 */
uint8_t mifare_get_block_count(uint8_t sector, MifareCardType type);

/**
 * Calculate CRC16 for MIFARE
 * @param data Data buffer
 * @param len Data length
 * @return CRC16 value
 */
uint16_t mifare_crc16(const uint8_t* data, uint32_t len);

// ========== Common Default Keys ==========

// Industry-standard default keys
extern const uint8_t MIFARE_KEY_DEFAULT_FF[6];  // FF FF FF FF FF FF
extern const uint8_t MIFARE_KEY_DEFAULT_00[6];  // 00 00 00 00 00 00
extern const uint8_t MIFARE_KEY_MAD[6];         // A0 A1 A2 A3 A4 A5
extern const uint8_t MIFARE_KEY_NFC[6];         // D3 F7 D3 F7 D3 F7

// Hotel industry keys
extern const uint8_t MIFARE_KEY_HOTEL_1[6];     // Common hotel key 1
extern const uint8_t MIFARE_KEY_HOTEL_2[6];     // Common hotel key 2
extern const uint8_t MIFARE_KEY_HOTEL_3[6];     // Common hotel key 3

// Building access keys
extern const uint8_t MIFARE_KEY_BUILDING_1[6];
extern const uint8_t MIFARE_KEY_BUILDING_2[6];

// Parking system keys
extern const uint8_t MIFARE_KEY_PARKING_1[6];
extern const uint8_t MIFARE_KEY_PARKING_2[6];

/**
 * AUTHORIZATION WARNING:
 * 
 * MIFARE Classic attacks must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Hotel key card cloning without authorization
 * - Unauthorized building access
 * - Parking system fraud
 * - Any form of trespassing or theft
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own RFID systems
 * - Authorized penetration testing with contracts
 * - Security research in lab environments
 * - Educational purposes with proper oversight
 * 
 * Unauthorized RFID cloning is a FELONY in most jurisdictions.
 */
