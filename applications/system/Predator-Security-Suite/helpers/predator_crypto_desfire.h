#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * MIFARE DESFire EV1/EV2/EV3 Implementation
 * 
 * Modern contactless smart card for access control and payment systems.
 * Replaces MIFARE Classic in 40%+ of new installations worldwide.
 * 
 * Used in: Hotels, airports, corporate offices, public transit, government facilities
 * 
 * Security: AES-128, 3DES, 3K3DES encryption with session keys and CMAC
 * 
 * CRITICAL: Obtain written authorization before testing any DESFire systems.
 */

// Forward declare app
struct PredatorApp;

// DESFire versions
typedef enum {
    DESFireEV1,     // 2006: 2KB/4KB/8KB variants
    DESFireEV2,     // 2013: Enhanced security, random UID
    DESFireEV3      // 2018: Latest with AES-128 only
} DESFireVersion;

// Authentication methods
typedef enum {
    DESFireAuthLegacyDES,       // DES (deprecated, EV1 only)
    DESFireAuth3DES,            // 3DES (2-key)
    DESFireAuth3K3DES,          // 3DES (3-key)
    DESFireAuthAES128,          // AES-128 (most common)
    DESFireAuthEV2              // EV2 first auth with PCD/PICC caps
} DESFireAuthMethod;

// Communication modes
typedef enum {
    DESFireCommPlain,           // No encryption
    DESFireCommMACed,           // MAC protected
    DESFireCommEncrypted        // Fully encrypted
} DESFireCommMode;

// File types
typedef enum {
    DESFireFileStandard,        // Standard data file
    DESFireFileBackup,          // Backup data file (transaction safe)
    DESFireFileValue,           // Value file (counter)
    DESFireFileLinearRecord,    // Linear record file
    DESFireFileCyclicRecord     // Cyclic record file
} DESFireFileType;

// DESFire card structure
typedef struct {
    DESFireVersion version;
    uint8_t uid[7];             // 7-byte UID
    uint8_t uid_len;
    uint8_t hardware_info[7];   // Hardware version info
    uint8_t software_info[7];   // Software version info
    uint32_t free_memory;       // Available EEPROM
    bool random_uid;            // EV2+ feature
} DESFireCard;

// DESFire authentication context
typedef struct {
    DESFireAuthMethod auth_method;
    uint8_t key_number;         // Key number (0-13)
    uint8_t key[24];            // Key data (16 bytes for AES, 24 for 3K3DES)
    uint8_t key_len;            // Key length
    
    // Session keys
    uint8_t session_key_enc[16];     // Encryption session key
    uint8_t session_key_mac[16];     // MAC session key
    uint8_t iv[16];                  // IV for CBC mode
    
    // Authentication state
    bool authenticated;
    uint32_t transaction_id;
    uint8_t cmac[16];           // Current CMAC
    
    // Communication mode
    DESFireCommMode comm_mode;
} DESFireContext;

// Application structure
typedef struct {
    uint32_t aid;               // Application ID (3 bytes)
    uint8_t key_settings;       // Key configuration
    uint8_t num_keys;           // Number of keys (1-14)
    uint8_t file_ids[32];       // File IDs in this application
    uint8_t file_count;         // Number of files
} DESFireApplication;

// File settings
typedef struct {
    DESFireFileType file_type;
    DESFireCommMode comm_mode;
    uint16_t access_rights;     // Read/Write/ReadWrite/ChangeAccessRights keys
    uint32_t file_size;         // For data files
    int32_t lower_limit;        // For value files
    int32_t upper_limit;        // For value files
    int32_t value;              // Current value
    uint8_t limited_credit;     // Limited credit enabled
    uint32_t record_size;       // For record files
    uint32_t max_records;       // For record files
    uint32_t current_records;   // Current number of records
} DESFireFileSettings;

// ========== Card-Level Commands ==========

/**
 * Detect DESFire card and read version
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if card detected
 */
bool desfire_detect_card(struct PredatorApp* app, DESFireCard* card);

/**
 * Get card UID
 * @param app PredatorApp context
 * @param uid Output UID buffer (7 bytes)
 * @return true if successful
 */
bool desfire_get_uid(struct PredatorApp* app, uint8_t* uid);

/**
 * Get version information
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if successful
 */
bool desfire_get_version(struct PredatorApp* app, DESFireCard* card);

/**
 * Format card (WARNING: Erases all data)
 * @param app PredatorApp context
 * @return true if successful
 */
bool desfire_format_card(struct PredatorApp* app);

/**
 * Get free memory
 * @param app PredatorApp context
 * @param free_bytes Output free memory
 * @return true if successful
 */
bool desfire_get_free_memory(struct PredatorApp* app, uint32_t* free_bytes);

// ========== Authentication ==========

/**
 * Authenticate with AES-128 (most common)
 * @param app PredatorApp context
 * @param ctx DESFire context with key
 * @param key_number Key number (0-13)
 * @return true if authenticated
 */
bool desfire_authenticate_aes(struct PredatorApp* app, DESFireContext* ctx, uint8_t key_number);

/**
 * Authenticate with 3DES
 * @param app PredatorApp context
 * @param ctx DESFire context with key
 * @param key_number Key number
 * @return true if authenticated
 */
bool desfire_authenticate_3des(struct PredatorApp* app, DESFireContext* ctx, uint8_t key_number);

/**
 * Authenticate with 3K3DES (3-key triple DES)
 * @param app PredatorApp context
 * @param ctx DESFire context with key
 * @param key_number Key number
 * @return true if authenticated
 */
bool desfire_authenticate_3k3des(struct PredatorApp* app, DESFireContext* ctx, uint8_t key_number);

/**
 * Authenticate EV2 first (with PCD capabilities)
 * @param app PredatorApp context
 * @param ctx DESFire context
 * @param key_number Key number
 * @return true if authenticated
 */
bool desfire_authenticate_ev2_first(struct PredatorApp* app, DESFireContext* ctx, uint8_t key_number);

// ========== Application Management ==========

/**
 * Get application IDs (AIDs)
 * @param app PredatorApp context
 * @param aids Output AID array
 * @param max_aids Maximum AIDs
 * @return Number of AIDs found
 */
uint32_t desfire_get_application_ids(struct PredatorApp* app, uint32_t* aids, uint32_t max_aids);

/**
 * Select application
 * @param app PredatorApp context
 * @param aid Application ID (3 bytes)
 * @return true if selected
 */
bool desfire_select_application(struct PredatorApp* app, uint32_t aid);

/**
 * Create application
 * @param app PredatorApp context
 * @param aid Application ID
 * @param key_settings Key configuration
 * @param num_keys Number of keys (1-14)
 * @return true if created
 */
bool desfire_create_application(struct PredatorApp* app, uint32_t aid, uint8_t key_settings, uint8_t num_keys);

/**
 * Delete application
 * @param app PredatorApp context
 * @param aid Application ID
 * @return true if deleted
 */
bool desfire_delete_application(struct PredatorApp* app, uint32_t aid);

/**
 * Get key settings for current application
 * @param app PredatorApp context
 * @param key_settings Output key settings
 * @param num_keys Output number of keys
 * @return true if successful
 */
bool desfire_get_key_settings(struct PredatorApp* app, uint8_t* key_settings, uint8_t* num_keys);

// ========== File Operations ==========

/**
 * Get file IDs in current application
 * @param app PredatorApp context
 * @param file_ids Output file ID array
 * @param max_files Maximum files
 * @return Number of files found
 */
uint32_t desfire_get_file_ids(struct PredatorApp* app, uint8_t* file_ids, uint32_t max_files);

/**
 * Get file settings
 * @param app PredatorApp context
 * @param file_id File ID
 * @param settings Output file settings
 * @return true if successful
 */
bool desfire_get_file_settings(struct PredatorApp* app, uint8_t file_id, DESFireFileSettings* settings);

/**
 * Create standard data file
 * @param app PredatorApp context
 * @param file_id File ID (0-31)
 * @param comm_mode Communication mode
 * @param access_rights Access rights (2 bytes)
 * @param file_size File size in bytes
 * @return true if created
 */
bool desfire_create_std_data_file(struct PredatorApp* app, uint8_t file_id, 
                                  DESFireCommMode comm_mode, uint16_t access_rights, uint32_t file_size);

/**
 * Read data from file
 * @param app PredatorApp context
 * @param ctx DESFire context (for decryption)
 * @param file_id File ID
 * @param offset Offset in file
 * @param length Length to read
 * @param data Output buffer
 * @return Number of bytes read (0 on error)
 */
uint32_t desfire_read_data(struct PredatorApp* app, DESFireContext* ctx, 
                           uint8_t file_id, uint32_t offset, uint32_t length, uint8_t* data);

/**
 * Write data to file
 * @param app PredatorApp context
 * @param ctx DESFire context (for encryption)
 * @param file_id File ID
 * @param offset Offset in file
 * @param length Length to write
 * @param data Data to write
 * @return true if successful
 */
bool desfire_write_data(struct PredatorApp* app, DESFireContext* ctx,
                       uint8_t file_id, uint32_t offset, uint32_t length, const uint8_t* data);

/**
 * Delete file
 * @param app PredatorApp context
 * @param file_id File ID
 * @return true if deleted
 */
bool desfire_delete_file(struct PredatorApp* app, uint8_t file_id);

// ========== Value File Operations ==========

/**
 * Get value from value file
 * @param app PredatorApp context
 * @param ctx DESFire context
 * @param file_id File ID
 * @param value Output value
 * @return true if successful
 */
bool desfire_get_value(struct PredatorApp* app, DESFireContext* ctx, uint8_t file_id, int32_t* value);

/**
 * Credit value file (add to value)
 * @param app PredatorApp context
 * @param ctx DESFire context
 * @param file_id File ID
 * @param amount Amount to add
 * @return true if successful
 */
bool desfire_credit(struct PredatorApp* app, DESFireContext* ctx, uint8_t file_id, int32_t amount);

/**
 * Debit value file (subtract from value)
 * @param app PredatorApp context
 * @param ctx DESFire context
 * @param file_id File ID
 * @param amount Amount to subtract
 * @return true if successful
 */
bool desfire_debit(struct PredatorApp* app, DESFireContext* ctx, uint8_t file_id, int32_t amount);

// ========== Security & Attack Functions ==========

/**
 * Dictionary attack - try known keys
 * @param app PredatorApp context
 * @param card Target card
 * @param found_keys Output found keys
 * @param max_keys Maximum keys to store
 * @return Number of keys found
 */
uint32_t desfire_attack_dictionary(struct PredatorApp* app, DESFireCard* card, 
                                   DESFireContext* found_keys, uint32_t max_keys);

/**
 * Dump all applications and files
 * @param app PredatorApp context
 * @param ctx Authenticated context
 * @param output_path Path to save dump
 * @return true if successful
 */
bool desfire_dump_card(struct PredatorApp* app, DESFireContext* ctx, const char* output_path);

/**
 * Clone DESFire card
 * @param app PredatorApp context
 * @param source Source card data
 * @param target Target writable card
 * @return true if cloned
 */
bool desfire_clone_card(struct PredatorApp* app, DESFireCard* source, DESFireCard* target);

// ========== Utilities ==========

/**
 * Calculate CRC32 for DESFire
 * @param data Data buffer
 * @param len Data length
 * @return CRC32 value
 */
uint32_t desfire_crc32(const uint8_t* data, uint32_t len);

/**
 * Calculate CMAC (Cipher-based MAC)
 * @param key Session key
 * @param data Data to authenticate
 * @param len Data length
 * @param cmac Output CMAC (16 bytes)
 * @return true if successful
 */
bool desfire_cmac(const uint8_t* key, const uint8_t* data, uint32_t len, uint8_t* cmac);

/**
 * Generate session keys from authentication
 * @param ctx DESFire context with master key
 * @param rnd_a Reader random
 * @param rnd_b Card random
 * @return true if successful
 */
bool desfire_generate_session_keys(DESFireContext* ctx, const uint8_t* rnd_a, const uint8_t* rnd_b);

// ========== Default Keys ==========

// Factory default keys (publicly known - ALWAYS change these!)
extern const uint8_t DESFIRE_KEY_DEFAULT_AES[16];      // All zeros
extern const uint8_t DESFIRE_KEY_DEFAULT_3DES[16];     // All zeros
extern const uint8_t DESFIRE_KEY_DEFAULT_3K3DES[24];   // All zeros

// Common hotel/building keys (should be changed but often aren't)
extern const uint8_t DESFIRE_KEY_HOTEL_MASTER[16];
extern const uint8_t DESFIRE_KEY_BUILDING_COMMON[16];

/**
 * AUTHORIZATION WARNING:
 * 
 * MIFARE DESFire attacks must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized hotel key cloning
 * - Unauthorized building/office access
 * - Transit fare evasion
 * - Payment fraud
 * - Any form of trespassing or theft
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own DESFire systems
 * - Authorized penetration testing with contracts
 * - Security research in controlled environments
 * - Educational purposes with proper oversight
 * 
 * Unauthorized DESFire attacks are a FELONY in most jurisdictions.
 * May violate computer fraud, trespass, burglary, and fraud laws.
 */
