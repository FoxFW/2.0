#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Sony FeliCa (NFC-F / JIS X 6319-4) Implementation
 * 
 * Dominant NFC standard in Asia-Pacific region.
 * 127 million users in Japan, major presence in Hong Kong, Singapore, Thailand.
 * 
 * Used in:
 * - Japan: Suica, Pasmo, ICOCA, Nimoca, Kitaca, TOICA (transit & payment)
 * - Hong Kong: Octopus Card
 * - Singapore: EZ-Link
 * - Mobile: Apple Pay/Google Pay in Japan
 * 
 * 60%+ of ALL NFC transactions in Asia-Pacific use FeliCa.
 * 
 * CRITICAL: Obtain written authorization before testing any FeliCa systems.
 */

// Forward declare app
struct PredatorApp;

// FeliCa operates at 13.56 MHz but uses different protocol than ISO 14443
// Uses Manchester encoding at 212/424 kbps

// Common system codes
#define FELICA_SYSTEM_COMMON          0xFE00  // Common area
#define FELICA_SYSTEM_SUICA           0x0003  // JR East Suica
#define FELICA_SYSTEM_PASMO           0x0003  // Pasmo (shares with Suica)
#define FELICA_SYSTEM_ICOCA           0x0003  // JR West ICOCA
#define FELICA_SYSTEM_EDY             0xFE00  // Edy e-money
#define FELICA_SYSTEM_NANACO          0xFE00  // nanaco
#define FELICA_SYSTEM_WAON            0xFE00  // WAON
#define FELICA_SYSTEM_OCTOPUS         0x8008  // Hong Kong Octopus

// Service codes (common)
#define FELICA_SERVICE_SUICA_HISTORY  0x090F  // Suica transaction history
#define FELICA_SERVICE_SUICA_BALANCE  0x008B  // Suica balance
#define FELICA_SERVICE_EDY_HISTORY    0x170F  // Edy transaction history
#define FELICA_SERVICE_EDY_BALANCE    0x1317  // Edy balance

// Card types
typedef enum {
    FeliCa_Unknown,
    FeliCa_Suica,                // JR East (Tokyo area)
    FeliCa_Pasmo,                // Private railways (Tokyo)
    FeliCa_ICOCA,                // JR West (Osaka area)
    FeliCa_Nimoca,               // Nishitetsu (Fukuoka)
    FeliCa_Kitaca,               // JR Hokkaido (Sapporo)
    FeliCa_TOICA,                // JR Central (Nagoya)
    FeliCa_SUGOCA,               // JR Kyushu (Fukuoka)
    FeliCa_Edy,                  // Rakuten Edy e-money
    FeliCa_nanaco,               // Seven-Eleven nanaco
    FeliCa_WAON,                 // Aeon WAON
    FeliCa_Octopus,              // Hong Kong Octopus
    FeliCa_EZLink,               // Singapore EZ-Link
    FeliCa_Mobile                // Mobile FeliCa (phone)
} FeliCaCardType;

// Authentication modes
typedef enum {
    FeliCa_Auth_None,            // No authentication (rare)
    FeliCa_Auth_2Way,            // 2-way mutual authentication
    FeliCa_Auth_3Way,            // 3-way mutual authentication (most common)
    FeliCa_Auth_4Way,            // 4-way mutual authentication (high security)
    FeliCa_Auth_AES              // AES authentication (mobile FeliCa)
} FeliCaAuthMode;

// Card structure
typedef struct {
    uint8_t idm[8];              // IDm (Manufacturer ID, like UID)
    uint8_t pmm[8];              // PMm (Manufacturer Parameters)
    uint16_t system_code;        // System code
    uint16_t service_codes[16];  // Service codes (areas on card)
    uint8_t service_count;
    FeliCaCardType card_type;
    bool is_mobile;              // Mobile FeliCa (phone)
    bool authenticated;
} FeliCaCard;

// Authentication context
typedef struct {
    uint8_t card_key[16];        // Card key (3DES, 16 bytes = 2-key 3DES)
    uint8_t session_key[16];     // Session key after authentication
    uint8_t rc[8];               // Random challenge from card
    uint8_t rr[8];               // Random response from reader
    FeliCaAuthMode auth_mode;
    bool authenticated;
} FeliCaAuthContext;

// Transaction history entry
typedef struct {
    uint8_t terminal_id[4];      // Terminal/Station ID
    uint8_t transaction_type;    // 0x01=debit, 0x02=credit, 0x03=purchase
    uint16_t amount;             // Transaction amount (yen/cents)
    uint16_t balance_after;      // Balance after transaction
    uint8_t date[3];             // Date (YY-MM-DD)
    uint8_t time[2];             // Time (HH:MM)
    uint8_t region_code;         // Region code
} FeliCaTransaction;

// ========== Detection & Identification ==========

/**
 * Detect FeliCa card (Polling command)
 * @param app PredatorApp context
 * @param system_code System code to search (0xFFFF = all)
 * @param card Output card structure
 * @return true if card detected
 */
bool felica_detect_card(struct PredatorApp* app, uint16_t system_code, FeliCaCard* card);

/**
 * Read IDm and PMm
 * @param app PredatorApp context
 * @param idm Output IDm (8 bytes)
 * @param pmm Output PMm (8 bytes)
 * @return true if successful
 */
bool felica_read_idm_pmm(struct PredatorApp* app, uint8_t* idm, uint8_t* pmm);

/**
 * Request system code (find all systems on card)
 * @param app PredatorApp context
 * @param card Card structure with IDm
 * @param system_codes Output system codes array
 * @param max_systems Maximum systems
 * @return Number of systems found
 */
uint32_t felica_request_system_code(struct PredatorApp* app, const FeliCaCard* card,
                                    uint16_t* system_codes, uint32_t max_systems);

/**
 * Search service code (find services in a system)
 * @param app PredatorApp context
 * @param card Card structure with IDm
 * @param service_codes Output service codes
 * @param max_services Maximum services
 * @return Number of services found
 */
uint32_t felica_search_service_code(struct PredatorApp* app, const FeliCaCard* card,
                                    uint16_t* service_codes, uint32_t max_services);

/**
 * Identify card type from IDm and system code
 * @param card Card structure
 * @return Detected card type
 */
FeliCaCardType felica_identify_card(const FeliCaCard* card);

// ========== Authentication ==========

/**
 * Mutual authentication (3-way, most common)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context with card key
 * @return true if authenticated
 */
bool felica_authenticate_mutual(struct PredatorApp* app, const FeliCaCard* card,
                                FeliCaAuthContext* auth_ctx);

/**
 * 2-way authentication (simpler, less secure)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context
 * @return true if authenticated
 */
bool felica_authenticate_2way(struct PredatorApp* app, const FeliCaCard* card,
                              FeliCaAuthContext* auth_ctx);

/**
 * Generate session key from authentication
 * @param auth_ctx Authentication context
 * @param rc Random challenge from card
 * @param rr Random response from reader
 * @return true if successful
 */
bool felica_generate_session_key(FeliCaAuthContext* auth_ctx, 
                                 const uint8_t* rc, const uint8_t* rr);

// ========== Read Operations ==========

/**
 * Read without encryption (public data)
 * @param app PredatorApp context
 * @param card Card structure
 * @param service_code Service code to read
 * @param block_list Block numbers to read
 * @param block_count Number of blocks
 * @param data Output data buffer
 * @return Number of blocks read
 */
uint32_t felica_read_without_encryption(struct PredatorApp* app, const FeliCaCard* card,
                                       uint16_t service_code, const uint8_t* block_list,
                                       uint8_t block_count, uint8_t* data);

/**
 * Read balance (for transit/payment cards)
 * @param app PredatorApp context
 * @param card Card structure
 * @param balance Output balance (in yen/cents)
 * @return true if successful
 */
bool felica_read_balance(struct PredatorApp* app, const FeliCaCard* card, uint16_t* balance);

/**
 * Read transaction history
 * @param app PredatorApp context
 * @param card Card structure
 * @param transactions Output transaction array
 * @param max_transactions Maximum transactions
 * @return Number of transactions read
 */
uint32_t felica_read_history(struct PredatorApp* app, const FeliCaCard* card,
                             FeliCaTransaction* transactions, uint32_t max_transactions);

/**
 * Read Suica-specific data
 * @param app PredatorApp context
 * @param card Card structure
 * @param balance Output balance
 * @param transactions Output transaction history
 * @param max_transactions Maximum transactions
 * @return Number of transactions read
 */
uint32_t felica_read_suica_data(struct PredatorApp* app, const FeliCaCard* card,
                               uint16_t* balance, FeliCaTransaction* transactions,
                               uint32_t max_transactions);

// ========== Write Operations ==========

/**
 * Write without encryption (requires authentication for most cards)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authenticated context
 * @param service_code Service code
 * @param block_list Block numbers
 * @param block_count Number of blocks
 * @param data Data to write
 * @return true if successful
 */
bool felica_write_without_encryption(struct PredatorApp* app, const FeliCaCard* card,
                                     const FeliCaAuthContext* auth_ctx,
                                     uint16_t service_code, const uint8_t* block_list,
                                     uint8_t block_count, const uint8_t* data);

// ========== Security Research ==========

/**
 * Dictionary attack on card key
 * @param app PredatorApp context
 * @param card Card structure
 * @param found_key Output key if found
 * @return true if key found
 */
bool felica_attack_dictionary(struct PredatorApp* app, const FeliCaCard* card,
                              uint8_t* found_key);

/**
 * Analyze card security features
 * @param app PredatorApp context
 * @param card Card structure
 * @param report Output security report (text buffer, min 512 bytes)
 * @return true if successful
 */
bool felica_analyze_security(struct PredatorApp* app, const FeliCaCard* card,
                             char* report);

/**
 * Dump card data (all readable blocks)
 * @param app PredatorApp context
 * @param card Card structure
 * @param output_path Path to save dump
 * @return true if successful
 */
bool felica_dump_card(struct PredatorApp* app, const FeliCaCard* card,
                     const char* output_path);

// ========== Utilities ==========

/**
 * Calculate FeliCa checksum
 * @param data Data buffer
 * @param len Data length
 * @return Checksum value
 */
uint16_t felica_checksum(const uint8_t* data, uint32_t len);

/**
 * Parse transaction data
 * @param raw_data Raw transaction data (16 bytes)
 * @param transaction Output transaction structure
 * @param card_type Card type (for format interpretation)
 * @return true if successful
 */
bool felica_parse_transaction(const uint8_t* raw_data, FeliCaTransaction* transaction,
                              FeliCaCardType card_type);

/**
 * Format transaction for display
 * @param transaction Transaction structure
 * @param output Output string buffer (min 128 chars)
 * @param card_type Card type
 */
void felica_format_transaction(const FeliCaTransaction* transaction, char* output,
                               FeliCaCardType card_type);

/**
 * Get card type name
 * @param type Card type
 * @return Human-readable name
 */
const char* felica_get_card_name(FeliCaCardType type);

/**
 * Decode station ID (for Suica/Pasmo)
 * @param terminal_id Terminal ID (4 bytes)
 * @param station_name Output station name (Japanese romanization)
 * @param max_len Maximum length
 * @return true if station found in database
 */
bool felica_decode_station_id(const uint8_t* terminal_id, char* station_name,
                              size_t max_len);

/**
 * Convert system code to application name
 * @param system_code System code
 * @return Application name
 */
const char* felica_system_code_to_string(uint16_t system_code);

// ========== Cryptographic Functions ==========

/**
 * 3DES encryption for FeliCa authentication
 * @param key 3DES key (16 bytes, 2-key)
 * @param data Data to encrypt (8 bytes)
 * @param output Output buffer (8 bytes)
 * @return true if successful
 */
bool felica_3des_encrypt(const uint8_t* key, const uint8_t* data, uint8_t* output);

/**
 * 3DES decryption
 * @param key 3DES key (16 bytes)
 * @param data Data to decrypt (8 bytes)
 * @param output Output buffer (8 bytes)
 * @return true if successful
 */
bool felica_3des_decrypt(const uint8_t* key, const uint8_t* data, uint8_t* output);

// ========== Known Keys & Defaults ==========

// Most FeliCa cards use diversified keys (derived from master key + IDm)
// These are research/default keys only

extern const uint8_t FELICA_KEY_DEFAULT[16];
extern const uint8_t FELICA_KEY_RESEARCH_SAMPLE[16];

/**
 * Derive card key from master key and IDm (card-specific)
 * @param master_key Master key (16 bytes)
 * @param idm Card IDm (8 bytes)
 * @param card_key Output card key (16 bytes)
 * @return true if successful
 */
bool felica_derive_card_key(const uint8_t* master_key, const uint8_t* idm,
                            uint8_t* card_key);

/**
 * AUTHORIZATION WARNING:
 * 
 * FeliCa attacks must ONLY be used on cards you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Fare evasion on transit systems
 * - Unauthorized balance manipulation
 * - Cloning payment cards
 * - Identity theft or impersonation
 * - Any form of fraud or unauthorized access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own FeliCa cards
 * - Authorized security research with proper oversight
 * - Academic research in controlled environments
 * - Professional penetration testing with written contracts
 * 
 * Unauthorized FeliCa manipulation is a SERIOUS CRIME in Japan and other countries.
 * Japan's Unauthorized Computer Access Law and Fraud Prevention laws carry
 * severe penalties including imprisonment.
 * 
 * Transit fare evasion in Japan: Up to 3 years imprisonment + fines
 * Electronic payment fraud: Up to 10 years imprisonment
 */
