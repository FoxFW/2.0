#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * EM4305/EM4469 Writable RFID Tags
 * 
 * Most common writable 125kHz tags for cloning attacks.
 * Used in: Vehicle access, animal tracking, industrial access control
 * 
 * Compatible formats: EM4100, HID Prox, Indala, AWID, Paradox
 * 
 * CRITICAL: For authorized security research and testing only.
 */

// Forward declare app
struct PredatorApp;

// Tag types
typedef enum {
    EM4305_Mode_EM4100,          // EM4100 compatible (most common)
    EM4305_Mode_HID,             // HID Prox compatible
    EM4305_Mode_Indala,          // Indala compatible
    EM4305_Mode_AWID,            // AWID compatible
    EM4305_Mode_Paradox,         // Paradox compatible
    EM4305_Mode_Custom           // Custom configuration
} EM4305Mode;

// Modulation types
typedef enum {
    EM4305_Modulation_Manchester,
    EM4305_Modulation_BiPhase,
    EM4305_Modulation_PSK1,
    EM4305_Modulation_PSK2,
    EM4305_Modulation_PSK3,
    EM4305_Modulation_FSK1,
    EM4305_Modulation_FSK2
} EM4305Modulation;

// Data rates
typedef enum {
    EM4305_DataRate_RF8,         // RF/8
    EM4305_DataRate_RF16,        // RF/16
    EM4305_DataRate_RF32,        // RF/32
    EM4305_DataRate_RF40,        // RF/40
    EM4305_DataRate_RF50,        // RF/50
    EM4305_DataRate_RF64,        // RF/64
    EM4305_DataRate_RF100,       // RF/100
    EM4305_DataRate_RF128        // RF/128
} EM4305DataRate;

// Configuration structure
typedef struct {
    uint32_t password;           // 32-bit password (0 = no password)
    EM4305Mode mode;
    EM4305Modulation modulation;
    EM4305DataRate data_rate;
    uint8_t max_block;           // Maximum block number
    bool password_enabled;
    bool aor_enabled;            // Adaptive Output Regulation
    bool otpbit;                 // One-Time Programmable bit
    uint8_t config_word[4];      // Raw configuration word
} EM4305Config;

// Tag data structure
typedef struct {
    uint64_t card_data;          // Card data (up to 64 bits)
    uint8_t data_blocks[16][4];  // 16 blocks of 4 bytes each
    EM4305Config config;
    bool is_locked;              // OTP locked
} EM4305Tag;

// ========== Detection & Reading ==========

/**
 * Detect EM4305/EM4469 tag
 * @param app PredatorApp context
 * @param tag Output tag structure
 * @return true if tag detected
 */
bool em4305_detect_tag(struct PredatorApp* app, EM4305Tag* tag);

/**
 * Read configuration word
 * @param app PredatorApp context
 * @param config Output configuration
 * @return true if successful
 */
bool em4305_read_config(struct PredatorApp* app, EM4305Config* config);

/**
 * Read single block
 * @param app PredatorApp context
 * @param block_address Block address (0-15)
 * @param data Output data (4 bytes)
 * @return true if successful
 */
bool em4305_read_block(struct PredatorApp* app, uint8_t block_address, uint8_t* data);

/**
 * Read all blocks
 * @param app PredatorApp context
 * @param tag Output tag structure
 * @return true if successful
 */
bool em4305_read_all_blocks(struct PredatorApp* app, EM4305Tag* tag);

/**
 * Read as EM4100 format (for cloned tags)
 * @param app PredatorApp context
 * @param card_data Output card data (40 bits)
 * @return true if successful
 */
bool em4305_read_em4100(struct PredatorApp* app, uint64_t* card_data);

// ========== Writing & Programming ==========

/**
 * Write single block
 * @param app PredatorApp context
 * @param config Tag configuration (for password)
 * @param block_address Block address (0-15)
 * @param data Data to write (4 bytes)
 * @return true if successful
 */
bool em4305_write_block(struct PredatorApp* app, const EM4305Config* config,
                       uint8_t block_address, const uint8_t* data);

/**
 * Write configuration
 * @param app PredatorApp context
 * @param old_config Current configuration (for password)
 * @param new_config New configuration to write
 * @return true if successful
 */
bool em4305_write_config(struct PredatorApp* app, const EM4305Config* old_config,
                        const EM4305Config* new_config);

/**
 * Clone EM4100 card onto EM4305
 * @param app PredatorApp context
 * @param source_data EM4100 card data (40 bits)
 * @param target_config Target EM4305 configuration (for password)
 * @return true if successful
 */
bool em4305_clone_em4100(struct PredatorApp* app, uint64_t source_data,
                        const EM4305Config* target_config);

/**
 * Clone HID Prox card onto EM4305
 * @param app PredatorApp context
 * @param facility_code Facility code
 * @param card_number Card number
 * @param target_config Target EM4305 configuration
 * @return true if successful
 */
bool em4305_clone_hid(struct PredatorApp* app, uint16_t facility_code,
                     uint32_t card_number, const EM4305Config* target_config);

/**
 * Clone Indala card onto EM4305
 * @param app PredatorApp context
 * @param indala_data Indala card data
 * @param target_config Target EM4305 configuration
 * @return true if successful
 */
bool em4305_clone_indala(struct PredatorApp* app, uint64_t indala_data,
                        const EM4305Config* target_config);

// ========== Security Operations ==========

/**
 * Set password
 * @param app PredatorApp context
 * @param old_password Current password (0 if none)
 * @param new_password New password
 * @return true if successful
 */
bool em4305_set_password(struct PredatorApp* app, uint32_t old_password,
                        uint32_t new_password);

/**
 * Disable password protection
 * @param app PredatorApp context
 * @param current_password Current password
 * @return true if successful
 */
bool em4305_disable_password(struct PredatorApp* app, uint32_t current_password);

/**
 * Login with password (required before protected write operations)
 * @param app PredatorApp context
 * @param password Password
 * @return true if password correct
 */
bool em4305_login(struct PredatorApp* app, uint32_t password);

/**
 * Lock tag (OTP - One Time Programmable, PERMANENT!)
 * @param app PredatorApp context
 * @param config Configuration (for password)
 * @return true if successful
 */
bool em4305_lock_tag(struct PredatorApp* app, const EM4305Config* config);

// ========== Attack Functions ==========

/**
 * Brute force password attack
 * @param app PredatorApp context
 * @param start_password Starting password
 * @param end_password Ending password
 * @param found_password Output password if found
 * @param callback Progress callback (can be NULL)
 * @return true if password found
 */
bool em4305_bruteforce_password(struct PredatorApp* app, uint32_t start_password,
                               uint32_t end_password, uint32_t* found_password,
                               void (*callback)(uint32_t current, uint32_t total));

/**
 * Dictionary attack with common passwords
 * @param app PredatorApp context
 * @param found_password Output password if found
 * @return true if password found
 */
bool em4305_attack_dictionary(struct PredatorApp* app, uint32_t* found_password);

/**
 * Sniff EM4100 card for cloning
 * @param app PredatorApp context
 * @param card_data Output card data
 * @param timeout_ms Timeout in milliseconds
 * @return true if card detected
 */
bool em4305_sniff_em4100(struct PredatorApp* app, uint64_t* card_data,
                        uint32_t timeout_ms);

// ========== Configuration Helpers ==========

/**
 * Create EM4100 compatible configuration
 * @param config Output configuration
 */
void em4305_config_em4100(EM4305Config* config);

/**
 * Create HID Prox compatible configuration
 * @param config Output configuration
 */
void em4305_config_hid(EM4305Config* config);

/**
 * Create Indala compatible configuration
 * @param config Output configuration
 */
void em4305_config_indala(EM4305Config* config);

/**
 * Parse configuration word to structure
 * @param config_word Raw configuration word (4 bytes)
 * @param config Output configuration structure
 */
void em4305_parse_config(const uint8_t* config_word, EM4305Config* config);

/**
 * Build configuration word from structure
 * @param config Configuration structure
 * @param config_word Output configuration word (4 bytes)
 */
void em4305_build_config(const EM4305Config* config, uint8_t* config_word);

// ========== Utilities ==========

/**
 * Calculate EM4100 parity bits
 * @param data Card data (40 bits without parity)
 * @return Data with parity bits
 */
uint64_t em4305_calculate_em4100_parity(uint64_t data);

/**
 * Verify EM4100 parity
 * @param data Card data with parity
 * @return true if parity valid
 */
bool em4305_verify_em4100_parity(uint64_t data);

/**
 * Format EM4100 data for display
 * @param data Card data
 * @param output Output string buffer (min 16 chars)
 */
void em4305_format_em4100(uint64_t data, char* output);

/**
 * Calculate HID Prox data from facility code and card number
 * @param facility_code Facility code (8 or 16 bits)
 * @param card_number Card number (16 or 24 bits)
 * @param format HID format (26, 35, 37 bit)
 * @return Formatted HID data
 */
uint64_t em4305_calculate_hid_data(uint16_t facility_code, uint32_t card_number,
                                   uint8_t format);

/**
 * Parse HID data to facility code and card number
 * @param hid_data HID card data
 * @param format HID format (26, 35, 37 bit)
 * @param facility_code Output facility code
 * @param card_number Output card number
 * @return true if successful
 */
bool em4305_parse_hid_data(uint64_t hid_data, uint8_t format,
                          uint16_t* facility_code, uint32_t* card_number);

// ========== Common Default Passwords ==========

extern const uint32_t EM4305_PASSWORD_DEFAULT;
extern const uint32_t EM4305_PASSWORD_COMMON[];
extern const uint32_t EM4305_PASSWORD_COUNT;

/**
 * Load common passwords for dictionary attack
 * @param passwords Output password array
 * @param max_passwords Maximum passwords
 * @return Number of passwords loaded
 */
uint32_t em4305_load_common_passwords(uint32_t* passwords, uint32_t max_passwords);

/**
 * AUTHORIZATION WARNING:
 * 
 * EM4305/EM4469 tools must ONLY be used on tags you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized vehicle access
 * - Cloning access cards without authorization
 * - Animal tracking fraud or tampering
 * - Identity theft or impersonation
 * - Any form of unauthorized access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own RFID systems
 * - Authorized penetration testing with contracts
 * - Security research in controlled environments
 * - Educational purposes with proper oversight
 * - Backing up your own access cards
 * 
 * Unauthorized RFID cloning is a FELONY in most jurisdictions.
 * May violate computer fraud, trespass, and identity theft laws.
 */
