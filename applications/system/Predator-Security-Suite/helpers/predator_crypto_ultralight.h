#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * NXP MIFARE UltraLight & UltraLight C Implementation
 * 
 * Low-cost RFID cards used in transit systems and event tickets worldwide.
 * Billions of cards in circulation.
 * 
 * Major uses:
 * - Public Transit: London Oyster, Paris Metro (older), NYC MetroCard replacement
 * - Event Tickets: Concerts, sports, festivals
 * - Hotel Key Cards: Budget hotels
 * - Loyalty Cards: Retail, restaurants
 * 
 * UltraLight: No encryption (very weak security)
 * UltraLight C: 3DES encryption (better but still vulnerable)
 * UltraLight EV1: Enhanced features
 * 
 * CRITICAL: For authorized security testing only.
 */

// Forward declare app
struct PredatorApp;

// UltraLight variants
typedef enum {
    UltraLight_Unknown,
    UltraLight_Original,         // Original UltraLight (no crypto)
    UltraLight_C,                // UltraLight C (3DES authentication)
    UltraLight_EV1_MF0UL11,     // EV1 48 bytes
    UltraLight_EV1_MF0UL21,     // EV1 128 bytes
    UltraLight_NTAG203,          // NTAG203 (144 bytes, no crypto)
    UltraLight_NTAG210,          // NTAG210 (48 bytes, password)
    UltraLight_NTAG213,          // NTAG213 (144 bytes, password)
    UltraLight_NTAG215,          // NTAG215 (504 bytes, password, NFC Forum Type 2)
    UltraLight_NTAG216           // NTAG216 (888 bytes, password)
} UltraLightType;

// Memory sizes (pages)
#define ULTRALIGHT_PAGES_ORIGINAL    16    // 64 bytes
#define ULTRALIGHT_PAGES_C           48    // 192 bytes
#define ULTRALIGHT_PAGES_EV1_MF0UL11 20    // 80 bytes
#define ULTRALIGHT_PAGES_EV1_MF0UL21 41    // 164 bytes
#define ULTRALIGHT_PAGES_NTAG210     20    // 80 bytes
#define ULTRALIGHT_PAGES_NTAG213     45    // 180 bytes
#define ULTRALIGHT_PAGES_NTAG215     135   // 540 bytes
#define ULTRALIGHT_PAGES_NTAG216     231   // 924 bytes

// Page size is always 4 bytes
#define ULTRALIGHT_PAGE_SIZE 4

// Card structure
typedef struct {
    uint8_t uid[7];              // 7-byte UID
    uint8_t uid_len;             // UID length (4 or 7 bytes)
    UltraLightType type;
    uint16_t page_count;         // Number of pages
    uint16_t total_bytes;        // Total memory
    uint8_t version[8];          // Version info (EV1/NTAG only)
    bool has_3des;               // Has 3DES authentication (UltraLight C)
    bool has_password;           // Has password protection (EV1/NTAG)
    bool is_locked;              // Permanently locked
} UltraLightCard;

// Authentication context (UltraLight C only)
typedef struct {
    uint8_t des_key[16];         // 3DES key (2-key, 16 bytes)
    uint8_t iv[8];               // IV for CBC mode
    bool authenticated;
} UltraLightAuthContext;

// Password protection (EV1/NTAG)
typedef struct {
    uint32_t password;           // 32-bit password
    uint16_t pack;               // 16-bit password acknowledgment
    uint8_t authlim;             // Authentication limit attempts
    bool password_enabled;
} UltraLightPasswordProtection;

// Transit ticket structure (common format)
typedef struct {
    uint8_t ticket_type;         // Ticket type
    uint16_t remaining_trips;    // Trips remaining
    uint16_t balance;            // Balance (cents/pence/etc)
    uint8_t valid_from[3];       // Valid from date (YYMMDD)
    uint8_t valid_until[3];      // Valid until date (YYMMDD)
    uint8_t zones[4];            // Valid zones bitmask
    uint32_t serial_number;      // Ticket serial
    bool is_valid;
} UltraLightTicket;

// ========== Detection & Reading ==========

/**
 * Detect UltraLight card
 * @param app PredatorApp context
 * @param card Output card structure
 * @return true if UltraLight card detected
 */
bool ultralight_detect_card(struct PredatorApp* app, UltraLightCard* card);

/**
 * Get version information (EV1/NTAG only)
 * @param app PredatorApp context
 * @param card Card structure
 * @return true if successful
 */
bool ultralight_get_version(struct PredatorApp* app, UltraLightCard* card);

/**
 * Read signature (EV1/NTAG only, anti-cloning feature)
 * @param app PredatorApp context
 * @param signature Output signature (32 bytes)
 * @return true if successful
 */
bool ultralight_read_signature(struct PredatorApp* app, uint8_t* signature);

/**
 * Detect card type from version and memory size
 * @param card Card structure
 * @return Detected type
 */
UltraLightType ultralight_detect_type(const UltraLightCard* card);

// ========== Memory Operations ==========

/**
 * Read page (4 bytes)
 * @param app PredatorApp context
 * @param card Card structure
 * @param page_number Page number
 * @param data Output buffer (4 bytes)
 * @return true if successful
 */
bool ultralight_read_page(struct PredatorApp* app, const UltraLightCard* card,
                         uint8_t page_number, uint8_t* data);

/**
 * Read multiple pages (fast read)
 * @param app PredatorApp context
 * @param card Card structure
 * @param start_page First page
 * @param page_count Number of pages
 * @param data Output buffer (page_count * 4 bytes)
 * @return Number of pages read
 */
uint32_t ultralight_read_pages(struct PredatorApp* app, const UltraLightCard* card,
                               uint8_t start_page, uint8_t page_count, uint8_t* data);

/**
 * Write page (4 bytes)
 * @param app PredatorApp context
 * @param card Card structure
 * @param page_number Page number
 * @param data Data to write (4 bytes)
 * @return true if successful
 */
bool ultralight_write_page(struct PredatorApp* app, const UltraLightCard* card,
                           uint8_t page_number, const uint8_t* data);

/**
 * Read entire card memory
 * @param app PredatorApp context
 * @param card Card structure
 * @param data Output buffer (total_bytes)
 * @return Number of bytes read
 */
uint32_t ultralight_read_all(struct PredatorApp* app, const UltraLightCard* card,
                             uint8_t* data);

// ========== Authentication (UltraLight C) ==========

/**
 * Authenticate with 3DES (UltraLight C only)
 * @param app PredatorApp context
 * @param card Card structure
 * @param auth_ctx Authentication context with key
 * @return true if authenticated
 */
bool ultralight_authenticate_3des(struct PredatorApp* app, const UltraLightCard* card,
                                  UltraLightAuthContext* auth_ctx);

/**
 * Change 3DES key (requires current authentication)
 * @param app PredatorApp context
 * @param auth_ctx Authenticated context
 * @param new_key New 3DES key (16 bytes)
 * @return true if successful
 */
bool ultralight_change_3des_key(struct PredatorApp* app, const UltraLightAuthContext* auth_ctx,
                                const uint8_t* new_key);

// ========== Password Protection (EV1/NTAG) ==========

/**
 * Authenticate with password (EV1/NTAG)
 * @param app PredatorApp context
 * @param card Card structure
 * @param password 32-bit password
 * @param pack Output PACK if successful
 * @return true if password correct
 */
bool ultralight_password_auth(struct PredatorApp* app, const UltraLightCard* card,
                              uint32_t password, uint16_t* pack);

/**
 * Set password protection (EV1/NTAG)
 * @param app PredatorApp context
 * @param card Card structure
 * @param protection Password protection settings
 * @return true if successful
 */
bool ultralight_set_password(struct PredatorApp* app, const UltraLightCard* card,
                             const UltraLightPasswordProtection* protection);

/**
 * Get password protection counter
 * @param app PredatorApp context
 * @param attempts_remaining Output remaining attempts
 * @return true if successful
 */
bool ultralight_get_password_attempts(struct PredatorApp* app, uint8_t* attempts_remaining);

// ========== Locking ==========

/**
 * Lock page (permanent, cannot be undone!)
 * @param app PredatorApp context
 * @param card Card structure
 * @param page_number Page to lock
 * @return true if successful
 */
bool ultralight_lock_page(struct PredatorApp* app, const UltraLightCard* card,
                         uint8_t page_number);

/**
 * Lock entire card (permanent!)
 * @param app PredatorApp context
 * @param card Card structure
 * @return true if successful
 */
bool ultralight_lock_card(struct PredatorApp* app, const UltraLightCard* card);

// ========== Security Research ==========

/**
 * Dictionary attack on 3DES key (UltraLight C)
 * @param app PredatorApp context
 * @param card Card structure
 * @param found_key Output key if found (16 bytes)
 * @return true if key found
 */
bool ultralight_attack_3des_dictionary(struct PredatorApp* app, const UltraLightCard* card,
                                       uint8_t* found_key);

/**
 * Brute force password (EV1/NTAG)
 * @param app PredatorApp context
 * @param card Card structure
 * @param start_password Start password
 * @param end_password End password
 * @param found_password Output password if found
 * @param found_pack Output PACK if found
 * @return true if password found
 */
bool ultralight_bruteforce_password(struct PredatorApp* app, const UltraLightCard* card,
                                    uint32_t start_password, uint32_t end_password,
                                    uint32_t* found_password, uint16_t* found_pack);

/**
 * Dictionary attack on password (EV1/NTAG)
 * @param app PredatorApp context
 * @param card Card structure
 * @param found_password Output password if found
 * @param found_pack Output PACK if found
 * @return true if password found
 */
bool ultralight_attack_password_dictionary(struct PredatorApp* app, const UltraLightCard* card,
                                           uint32_t* found_password, uint16_t* found_pack);

/**
 * Dump card memory
 * @param app PredatorApp context
 * @param card Card structure
 * @param output_path Path to save dump
 * @return true if successful
 */
bool ultralight_dump_card(struct PredatorApp* app, const UltraLightCard* card,
                         const char* output_path);

/**
 * Clone card (copy to writable UltraLight)
 * @param app PredatorApp context
 * @param source Source card
 * @param target Target writable card
 * @return true if successful
 */
bool ultralight_clone_card(struct PredatorApp* app, const UltraLightCard* source,
                           UltraLightCard* target);

// ========== Transit Ticket Parsing ==========

/**
 * Parse transit ticket data
 * @param data Raw ticket data
 * @param len Data length
 * @param ticket Output ticket structure
 * @return true if successful
 */
bool ultralight_parse_ticket(const uint8_t* data, uint32_t len, UltraLightTicket* ticket);

/**
 * Format ticket for display
 * @param ticket Ticket structure
 * @param output Output string buffer (min 256 chars)
 */
void ultralight_format_ticket(const UltraLightTicket* ticket, char* output);

/**
 * Validate ticket (check dates, balance, etc.)
 * @param ticket Ticket structure
 * @return true if currently valid
 */
bool ultralight_is_ticket_valid(const UltraLightTicket* ticket);

// ========== Utilities ==========

/**
 * Calculate CRC for UltraLight commands
 * @param data Data buffer
 * @param len Data length
 * @return CRC value
 */
uint16_t ultralight_crc(const uint8_t* data, uint32_t len);

/**
 * Get card type name
 * @param type Card type
 * @return Human-readable name
 */
const char* ultralight_get_type_name(UltraLightType type);

/**
 * Get memory size for type
 * @param type Card type
 * @return Memory size in bytes
 */
uint16_t ultralight_get_memory_size(UltraLightType type);

/**
 * Check if type has 3DES
 * @param type Card type
 * @return true if has 3DES authentication
 */
bool ultralight_has_3des(UltraLightType type);

/**
 * Check if type has password protection
 * @param type Card type
 * @return true if has password protection
 */
bool ultralight_has_password(UltraLightType type);

// ========== 3DES Operations (UltraLight C) ==========

/**
 * 3DES encryption for UltraLight C
 * @param key 3DES key (16 bytes, 2-key)
 * @param iv IV (8 bytes)
 * @param data Data to encrypt (8 bytes)
 * @param output Output buffer (8 bytes)
 * @return true if successful
 */
bool ultralight_3des_encrypt(const uint8_t* key, const uint8_t* iv,
                             const uint8_t* data, uint8_t* output);

/**
 * 3DES decryption for UltraLight C
 * @param key 3DES key (16 bytes)
 * @param iv IV (8 bytes)
 * @param data Data to decrypt (8 bytes)
 * @param output Output buffer (8 bytes)
 * @return true if successful
 */
bool ultralight_3des_decrypt(const uint8_t* key, const uint8_t* iv,
                             const uint8_t* data, uint8_t* output);

// ========== Known Keys & Defaults ==========

// UltraLight C default keys
extern const uint8_t ULTRALIGHT_C_KEY_DEFAULT[16];
extern const uint8_t ULTRALIGHT_C_KEY_NDEF[16];

// Common passwords (EV1/NTAG)
extern const uint32_t ULTRALIGHT_PASSWORD_DEFAULT;
extern const uint32_t ULTRALIGHT_PASSWORD_COMMON[];
extern const uint32_t ULTRALIGHT_PASSWORD_COUNT;

/**
 * Load common 3DES keys for dictionary attack
 * @param keys Output key array
 * @param max_keys Maximum keys
 * @return Number of keys loaded
 */
uint32_t ultralight_load_common_3des_keys(uint8_t keys[][16], uint32_t max_keys);

/**
 * Load common passwords for dictionary attack
 * @param passwords Output password array
 * @param max_passwords Maximum passwords
 * @return Number of passwords loaded
 */
uint32_t ultralight_load_common_passwords(uint32_t* passwords, uint32_t max_passwords);

/**
 * AUTHORIZATION WARNING:
 * 
 * MIFARE UltraLight attacks must ONLY be used on cards you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Transit fare evasion
 * - Event ticket fraud
 * - Hotel key card unauthorized access
 * - Loyalty card fraud
 * - Any form of unauthorized access or fraud
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own UltraLight cards
 * - Authorized security research for transit authorities
 * - Academic research in controlled environments
 * - Professional penetration testing with written contracts
 * 
 * Unauthorized UltraLight manipulation is a CRIME in most jurisdictions.
 * 
 * UK: Fraud Act 2006
 * - Oyster card fraud: Up to 10 years imprisonment
 * 
 * US: Wire Fraud, Computer Fraud
 * - Transit fare evasion: Fines + criminal record
 * - Electronic ticket fraud: Up to 5 years imprisonment
 * 
 * EU: Fraud prevention laws vary by country but all carry severe penalties
 * 
 * Transit authorities actively prosecute fare evasion and ticket fraud.
 */
