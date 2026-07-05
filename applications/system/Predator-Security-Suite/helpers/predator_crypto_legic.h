#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Legic Prime & Advant Implementation
 * 
 * Swiss/German/Austrian standard for access control and public transit.
 * Proprietary Legic polynomial-based encryption.
 * 
 * Major implementations:
 * - Switzerland: Ski resorts (90%+ market), building access, parking
 * - Germany: Transit systems, universities, stadiums
 * - Austria: Transit and access control
 * 
 * ~25% of European access control market
 * Dominant in alpine regions (Switzerland, Austria, parts of Germany)
 * 
 * CRITICAL: Obtain written authorization before testing Legic systems.
 */

// Forward declare app
struct PredatorApp;

// Legic operates at 13.56 MHz with proprietary protocol
// Uses polynomial-based stream cipher (not standard crypto)

// Legic card types
typedef enum {
    Legic_Unknown,
    Legic_Prime,                 // Legic Prime (MIM256, MIM1024)
    Legic_Advant,                // Legic Advant (ATC256, ATC1024, ATC2048, ATC4096)
    Legic_Prime_ISO14443,        // Legic Prime with ISO 14443 wrapper
    Legic_Advant_ISO15693        // Legic Advant with ISO 15693 wrapper
} LegicCardType;

// Memory sizes
typedef enum {
    Legic_MIM22,                 // 22 bytes (very rare)
    Legic_MIM256,                // 256 bytes (most common Prime)
    Legic_MIM1024,               // 1024 bytes (Prime)
    Legic_ATC256,                // 256 bytes (Advant)
    Legic_ATC1024,               // 1024 bytes (Advant)
    Legic_ATC2048,               // 2048 bytes (Advant)
    Legic_ATC4096                // 4096 bytes (Advant, rare)
} LegicMemorySize;

// Credential types (application-specific)
typedef enum {
    Legic_Cred_SkiPass,          // Ski resort pass
    Legic_Cred_Transit,          // Public transit
    Legic_Cred_Access,           // Building access
    Legic_Cred_Parking,          // Parking systems
    Legic_Cred_University,       // University campus
    Legic_Cred_Generic           // Generic credential
} LegicCredentialType;

// Card structure
typedef struct {
    uint8_t uid[4];              // UID (4 bytes for Prime, varies for Advant)
    uint8_t uid_len;
    LegicCardType card_type;
    LegicMemorySize memory_size;
    uint16_t total_bytes;        // Total memory
    uint8_t stamp[7];            // Legic stamp (proprietary)
    uint32_t crc;                // Master CRC
    bool has_segment;            // Segmented memory
} LegicCard;

// Segment structure (Legic uses segmented memory)
typedef struct {
    uint8_t segment_id;          // Segment ID (0-3 for Prime)
    uint16_t offset;             // Segment offset in memory
    uint16_t length;             // Segment length
    uint8_t access_rights;       // Access rights
    uint8_t key_number;          // Key number for this segment
    bool is_encrypted;           // Encrypted segment
    bool is_readable;            // Readable without key
    bool is_writable;            // Writable without key
} LegicSegment;

// Credential structure
typedef struct {
    LegicCredentialType cred_type;
    uint8_t segment_id;          // Which segment contains credential
    uint32_t credential_number;  // Credential/ticket number
    uint8_t validity_start[3];   // Start date (YYMMDD)
    uint8_t validity_end[3];     // End date (YYMMDD)
    uint16_t access_rights;      // Access rights bitmask
    uint8_t user_data[64];       // User-specific data
    bool is_valid;
} LegicCredential;

// Authentication context
typedef struct {
    uint8_t master_token[5];     // Master Token (stamp-based)
    uint8_t segment_keys[4][7];  // Up to 4 segment keys (7 bytes each)
    uint8_t key_count;
    bool authenticated;
} LegicAuthContext;

// ========== Detection & Reading ==========

/**
 * Detect Legic card
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if Legic card detected
 */
bool legic_detect_card(struct PredatorApp* app, LegicCard* card);

/**
 * Read card UID and stamp
 * @param app PredatorApp context
 * @param uid Output UID buffer
 * @param uid_len Output UID length
 * @param stamp Output stamp (7 bytes)
 * @return true if successful
 */
bool legic_read_uid_stamp(struct PredatorApp* app, uint8_t* uid, uint8_t* uid_len,
                          uint8_t* stamp);

/**
 * Get memory configuration
 * @param app PredatorApp context
 * @param card Card structure
 * @return true if successful
 */
bool legic_get_memory_info(struct PredatorApp* app, LegicCard* card);

/**
 * Read segment information
 * @param app PredatorApp context
 * @param card Card structure
 * @param segment_id Segment ID (0-3)
 * @param segment Output segment structure
 * @return true if successful
 */
bool legic_read_segment_info(struct PredatorApp* app, const LegicCard* card,
                             uint8_t segment_id, LegicSegment* segment);

// ========== Memory Operations ==========

/**
 * Read bytes (unencrypted areas only without auth)
 * @param app PredatorApp context
 * @param card Card structure
 * @param offset Offset in memory
 * @param length Length to read
 * @param data Output buffer
 * @return Number of bytes read
 */
uint32_t legic_read_bytes(struct PredatorApp* app, const LegicCard* card,
                          uint16_t offset, uint16_t length, uint8_t* data);

/**
 * Read segment data (requires authentication for protected segments)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context (can be NULL for unprotected)
 * @param segment_id Segment ID
 * @param data Output buffer
 * @param max_len Maximum length
 * @return Number of bytes read
 */
uint32_t legic_read_segment(struct PredatorApp* app, const LegicCard* card,
                            const LegicAuthContext* auth_ctx, uint8_t segment_id,
                            uint8_t* data, uint32_t max_len);

/**
 * Write bytes (requires authentication)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authenticated context
 * @param offset Offset in memory
 * @param data Data to write
 * @param length Length to write
 * @return true if successful
 */
bool legic_write_bytes(struct PredatorApp* app, const LegicCard* card,
                       const LegicAuthContext* auth_ctx, uint16_t offset,
                       const uint8_t* data, uint16_t length);

// ========== Authentication ==========

/**
 * Authenticate with master token (stamp-based)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context
 * @return true if authenticated
 */
bool legic_authenticate_master(struct PredatorApp* app, const LegicCard* card,
                               LegicAuthContext* auth_ctx);

/**
 * Authenticate segment (segment-specific key)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context
 * @param segment_id Segment ID
 * @param segment_key Segment key (7 bytes)
 * @return true if authenticated
 */
bool legic_authenticate_segment(struct PredatorApp* app, const LegicCard* card,
                                LegicAuthContext* auth_ctx, uint8_t segment_id,
                                const uint8_t* segment_key);

/**
 * Generate master token from stamp (Legic proprietary algorithm)
 * @param stamp Card stamp (7 bytes)
 * @param master_key Master key (implementation-specific)
 * @param token Output token (5 bytes)
 * @return true if successful
 */
bool legic_generate_master_token(const uint8_t* stamp, const uint8_t* master_key,
                                 uint8_t* token);

// ========== Credential Reading ==========

/**
 * Read credential from segment
 * @param app PredatorApp context
 * @param card Card structure
 * @param segment_id Segment containing credential
 * @param credential Output credential structure
 * @return true if successful
 */
bool legic_read_credential(struct PredatorApp* app, const LegicCard* card,
                           uint8_t segment_id, LegicCredential* credential);

/**
 * Read all credentials on card
 * @param app PredatorApp context
 * @param card Card structure
 * @param credentials Output credential array
 * @param max_credentials Maximum credentials
 * @return Number of credentials found
 */
uint32_t legic_read_all_credentials(struct PredatorApp* app, const LegicCard* card,
                                    LegicCredential* credentials, uint32_t max_credentials);

/**
 * Check credential validity
 * @param credential Credential structure
 * @return true if currently valid
 */
bool legic_is_credential_valid(const LegicCredential* credential);

// ========== Security Research ==========

/**
 * Dictionary attack on segment keys
 * @param app PredatorApp context
 * @param card Card structure
 * @param segment_id Segment to attack
 * @param found_key Output key if found (7 bytes)
 * @return true if key found
 */
bool legic_attack_dictionary(struct PredatorApp* app, const LegicCard* card,
                             uint8_t segment_id, uint8_t* found_key);

/**
 * Analyze card security
 * @param app PredatorApp context
 * @param card Card structure
 * @param report Output security report (min 512 bytes)
 * @return true if successful
 */
bool legic_analyze_security(struct PredatorApp* app, const LegicCard* card,
                            char* report);

/**
 * Dump card memory
 * @param app PredatorApp context
 * @param card Card structure
 * @param output_path Path to save dump
 * @return true if successful
 */
bool legic_dump_card(struct PredatorApp* app, const LegicCard* card,
                    const char* output_path);

/**
 * Clone card (copy to writable Legic card)
 * @param app PredatorApp context
 * @param source Source card
 * @param target Target writable card
 * @param auth_ctx Authentication context
 * @return true if successful
 */
bool legic_clone_card(struct PredatorApp* app, const LegicCard* source,
                     LegicCard* target, const LegicAuthContext* auth_ctx);

// ========== Utilities ==========

/**
 * Calculate Legic CRC (proprietary polynomial)
 * @param data Data buffer
 * @param len Data length
 * @param polynomial Polynomial value
 * @return CRC value
 */
uint32_t legic_crc(const uint8_t* data, uint32_t len, uint32_t polynomial);

/**
 * Parse credential data
 * @param raw_data Raw credential data
 * @param len Data length
 * @param credential Output credential structure
 * @param cred_type Credential type hint
 * @return true if successful
 */
bool legic_parse_credential(const uint8_t* raw_data, uint32_t len,
                            LegicCredential* credential, LegicCredentialType cred_type);

/**
 * Format credential for display
 * @param credential Credential structure
 * @param output Output string buffer (min 256 chars)
 */
void legic_format_credential(const LegicCredential* credential, char* output);

/**
 * Get card type name
 * @param type Card type
 * @return Human-readable name
 */
const char* legic_get_card_name(LegicCardType type);

/**
 * Get memory size name
 * @param size Memory size
 * @return Human-readable name
 */
const char* legic_get_memory_name(LegicMemorySize size);

/**
 * Detect credential type from data pattern
 * @param data Credential data
 * @param len Data length
 * @return Detected credential type
 */
LegicCredentialType legic_detect_credential_type(const uint8_t* data, uint32_t len);

// ========== Legic Polynomial Operations ==========

/**
 * Legic polynomial stream cipher (proprietary)
 * @param input Input data
 * @param len Data length
 * @param polynomial Polynomial value
 * @param output Output buffer
 * @return true if successful
 */
bool legic_polynomial_cipher(const uint8_t* input, uint32_t len,
                             uint32_t polynomial, uint8_t* output);

/**
 * Calculate checksum for segment
 * @param segment_data Segment data
 * @param len Data length
 * @return Checksum value
 */
uint8_t legic_segment_checksum(const uint8_t* segment_data, uint32_t len);

// ========== Known Keys & Defaults ==========

// Legic uses proprietary algorithms, limited public key knowledge
// These are research/test keys only

extern const uint8_t LEGIC_MASTER_KEY_DEFAULT[7];
extern const uint8_t LEGIC_SEGMENT_KEY_DEFAULT[7];

/**
 * Load common Legic keys for dictionary attack
 * @param keys Output key array
 * @param max_keys Maximum keys
 * @return Number of keys loaded
 */
uint32_t legic_load_common_keys(uint8_t keys[][7], uint32_t max_keys);

/**
 * AUTHORIZATION WARNING:
 * 
 * Legic attacks must ONLY be used on cards you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Ski resort lift fraud (free skiing)
 * - Transit fare evasion
 * - Unauthorized building/parking access
 * - University credential fraud
 * - Any form of unauthorized access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own Legic cards
 * - Authorized security research for Legic system operators
 * - Academic research in controlled environments
 * - Professional penetration testing with written contracts
 * 
 * Unauthorized Legic manipulation is a SERIOUS CRIME in Switzerland, Germany, Austria.
 * 
 * Switzerland: Fraud laws carry severe penalties
 * - Ski resort fraud: CHF 10,000+ fines, criminal record
 * - Access control fraud: Up to 5 years imprisonment
 * 
 * Germany/Austria: Similar fraud prevention laws
 * 
 * Legic is heavily used in high-value systems (ski resorts, stadiums).
 * Prosecution is vigorous and penalties are severe.
 */
