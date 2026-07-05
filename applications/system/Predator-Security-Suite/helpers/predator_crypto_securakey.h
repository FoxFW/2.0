#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * SECURAKEY / Wiegand Protocol Implementation
 * 
 * North American proximity card standard for access control.
 * Used in 20%+ of US access control systems.
 * 
 * Compatible formats:
 * - SECURAKEY (26-bit, 36-bit, 37-bit)
 * - HID Prox (26-bit, 35-bit, 37-bit standard)
 * - AWID (various formats)
 * - Indala (26-bit, 29-bit)
 * - EM Prox (various)
 * 
 * Common in: Hospitals, universities, offices, hotels, government buildings
 * 
 * CRITICAL: For authorized security testing only.
 */

// Forward declare app
struct PredatorApp;

// Wiegand formats (bit lengths)
typedef enum {
    Wiegand_26bit,               // Most common (HID standard)
    Wiegand_34bit,               // HID Corporate 1000
    Wiegand_35bit,               // HID Corporate 1000 35-bit
    Wiegand_36bit,               // SECURAKEY standard
    Wiegand_37bit,               // HID H10304
    Wiegand_48bit,               // HID Extended
    Wiegand_Custom               // Custom format
} WiegandFormat;

// Card manufacturers
typedef enum {
    Wiegand_Unknown,
    Wiegand_HID,                 // HID Global (most common)
    Wiegand_AWID,                // AWID
    Wiegand_Indala,              // Motorola Indala
    Wiegand_SECURAKEY,           // SECURAKEY
    Wiegand_EMProx,              // EM Proximity
    Wiegand_Casi,                // CASI-RUSCO
    Wiegand_Farpointe,           // Farpointe Data
    Wiegand_Kantech,             // Kantech
    Wiegand_Generic              // Generic Wiegand
} WiegandManufacturer;

// Card structure
typedef struct {
    uint64_t raw_data;           // Raw Wiegand data
    WiegandFormat format;
    WiegandManufacturer manufacturer;
    uint16_t facility_code;      // Facility code (8 or 16 bits)
    uint32_t card_number;        // Card number (16 or 24 bits)
    uint8_t bit_length;          // Actual bit length
    bool parity_valid;           // Parity check passed
} WiegandCard;

// Reader configuration
typedef struct {
    uint8_t pulse_width_us;      // Pulse width (typically 50-100μs)
    uint8_t pulse_interval_us;   // Interval between pulses (typically 1000-2000μs)
    bool use_26bit;              // Default to 26-bit format
    bool verify_parity;          // Verify parity bits
} WiegandReaderConfig;

// ========== Detection & Reading ==========

/**
 * Read Wiegand card
 * @param app PredatorApp context
 * @param card Output card structure
 * @param timeout_ms Timeout in milliseconds
 * @return true if card read
 */
bool wiegand_read_card(struct PredatorApp* app, WiegandCard* card, uint32_t timeout_ms);

/**
 * Detect Wiegand format from bit count and data pattern
 * @param raw_data Raw card data
 * @param bit_length Bit length
 * @return Detected format
 */
WiegandFormat wiegand_detect_format(uint64_t raw_data, uint8_t bit_length);

/**
 * Detect manufacturer from data pattern
 * @param card Card structure
 * @return Detected manufacturer
 */
WiegandManufacturer wiegand_detect_manufacturer(const WiegandCard* card);

// ========== Encoding / Decoding ==========

/**
 * Encode 26-bit HID Prox format
 * @param facility_code Facility code (8 bits)
 * @param card_number Card number (16 bits)
 * @return Encoded 26-bit data
 */
uint32_t wiegand_encode_26bit(uint8_t facility_code, uint16_t card_number);

/**
 * Decode 26-bit HID Prox format
 * @param data 26-bit data
 * @param facility_code Output facility code
 * @param card_number Output card number
 * @return true if successful
 */
bool wiegand_decode_26bit(uint32_t data, uint8_t* facility_code, uint16_t* card_number);

/**
 * Encode 35-bit HID Corporate 1000
 * @param facility_code Facility code (12 bits)
 * @param card_number Card number (20 bits)
 * @return Encoded 35-bit data
 */
uint64_t wiegand_encode_35bit(uint16_t facility_code, uint32_t card_number);

/**
 * Decode 35-bit format
 * @param data 35-bit data
 * @param facility_code Output facility code
 * @param card_number Output card number
 * @return true if successful
 */
bool wiegand_decode_35bit(uint64_t data, uint16_t* facility_code, uint32_t* card_number);

/**
 * Encode 37-bit HID H10304
 * @param facility_code Facility code (16 bits)
 * @param card_number Card number (19 bits)
 * @return Encoded 37-bit data
 */
uint64_t wiegand_encode_37bit(uint16_t facility_code, uint32_t card_number);

/**
 * Decode 37-bit format
 * @param data 37-bit data
 * @param facility_code Output facility code
 * @param card_number Output card number
 * @return true if successful
 */
bool wiegand_decode_37bit(uint64_t data, uint16_t* facility_code, uint32_t* card_number);

/**
 * Encode SECURAKEY 36-bit format
 * @param facility_code Facility code (16 bits)
 * @param card_number Card number (16 bits)
 * @return Encoded 36-bit data
 */
uint64_t wiegand_encode_securakey_36bit(uint16_t facility_code, uint16_t card_number);

/**
 * Decode SECURAKEY 36-bit format
 * @param data 36-bit data
 * @param facility_code Output facility code
 * @param card_number Output card number
 * @return true if successful
 */
bool wiegand_decode_securakey_36bit(uint64_t data, uint16_t* facility_code, uint16_t* card_number);

// ========== Parity Calculation ==========

/**
 * Calculate even parity for Wiegand
 * @param data Data bits
 * @param start_bit Start bit position
 * @param bit_count Number of bits
 * @return Parity bit (0 or 1)
 */
uint8_t wiegand_calculate_even_parity(uint64_t data, uint8_t start_bit, uint8_t bit_count);

/**
 * Calculate odd parity for Wiegand
 * @param data Data bits
 * @param start_bit Start bit position
 * @param bit_count Number of bits
 * @return Parity bit (0 or 1)
 */
uint8_t wiegand_calculate_odd_parity(uint64_t data, uint8_t start_bit, uint8_t bit_count);

/**
 * Verify 26-bit parity
 * @param data 26-bit data with parity
 * @return true if parity valid
 */
bool wiegand_verify_26bit_parity(uint32_t data);

/**
 * Add parity bits to raw data (for encoding)
 * @param data Raw data without parity
 * @param bit_length Bit length (excluding parity)
 * @param format Wiegand format
 * @return Data with parity bits
 */
uint64_t wiegand_add_parity(uint64_t data, uint8_t bit_length, WiegandFormat format);

// ========== Transmission (for cloning/emulation) ==========

/**
 * Transmit Wiegand data (bit-banging)
 * @param app PredatorApp context
 * @param card Card to transmit
 * @param config Reader configuration
 * @return true if successful
 */
bool wiegand_transmit(struct PredatorApp* app, const WiegandCard* card,
                     const WiegandReaderConfig* config);

/**
 * Emulate Wiegand card continuously
 * @param app PredatorApp context
 * @param card Card to emulate
 * @param duration_ms Duration in milliseconds (0 = until stopped)
 * @return true if successful
 */
bool wiegand_emulate_card(struct PredatorApp* app, const WiegandCard* card,
                         uint32_t duration_ms);

// ========== Cloning & Writing ==========

/**
 * Clone Wiegand card to writable 125kHz tag (EM4305, T5577)
 * @param app PredatorApp context
 * @param source Source Wiegand card
 * @param target_type Target tag type ("EM4305" or "T5577")
 * @return true if successful
 */
bool wiegand_clone_to_125khz(struct PredatorApp* app, const WiegandCard* source,
                             const char* target_type);

/**
 * Create custom Wiegand card
 * @param facility_code Facility code
 * @param card_number Card number
 * @param format Wiegand format
 * @param card Output card structure
 * @return true if successful
 */
bool wiegand_create_card(uint16_t facility_code, uint32_t card_number,
                        WiegandFormat format, WiegandCard* card);

// ========== Security Research ==========

/**
 * Brute force facility codes (try all facility codes for a building)
 * @param app PredatorApp context
 * @param start_facility Start facility code
 * @param end_facility End facility code
 * @param card_number Fixed card number to try
 * @param format Wiegand format
 * @param callback Callback for each attempt (can be NULL)
 * @return Number of successful access attempts
 */
uint32_t wiegand_bruteforce_facility(struct PredatorApp* app,
                                    uint16_t start_facility, uint16_t end_facility,
                                    uint32_t card_number, WiegandFormat format,
                                    void (*callback)(uint16_t facility, bool success));

/**
 * Brute force card numbers (try all card numbers for a facility)
 * @param app PredatorApp context
 * @param facility_code Facility code
 * @param start_card Start card number
 * @param end_card End card number
 * @param format Wiegand format
 * @param callback Callback for each attempt
 * @return Number of successful access attempts
 */
uint32_t wiegand_bruteforce_cards(struct PredatorApp* app, uint16_t facility_code,
                                  uint32_t start_card, uint32_t end_card,
                                  WiegandFormat format,
                                  void (*callback)(uint32_t card_num, bool success));

/**
 * Sniff Wiegand reader communication (between card and reader)
 * @param app PredatorApp context
 * @param cards Output card array
 * @param max_cards Maximum cards to capture
 * @param timeout_ms Timeout in milliseconds
 * @return Number of cards captured
 */
uint32_t wiegand_sniff_cards(struct PredatorApp* app, WiegandCard* cards,
                             uint32_t max_cards, uint32_t timeout_ms);

// ========== Utilities ==========

/**
 * Format card for display
 * @param card Card structure
 * @param output Output string buffer (min 64 chars)
 */
void wiegand_format_card(const WiegandCard* card, char* output);

/**
 * Get format name
 * @param format Wiegand format
 * @return Human-readable format name
 */
const char* wiegand_get_format_name(WiegandFormat format);

/**
 * Get manufacturer name
 * @param manufacturer Manufacturer
 * @return Human-readable manufacturer name
 */
const char* wiegand_get_manufacturer_name(WiegandManufacturer manufacturer);

/**
 * Convert raw data to bit string
 * @param data Raw data
 * @param bit_length Bit length
 * @param output Output string buffer (min 65 chars)
 */
void wiegand_to_bit_string(uint64_t data, uint8_t bit_length, char* output);

/**
 * Parse bit string to raw data
 * @param bit_string Bit string (e.g., "10110...")
 * @return Raw data
 */
uint64_t wiegand_from_bit_string(const char* bit_string);

/**
 * Get common facility codes for testing
 * @param manufacturer Manufacturer
 * @param facility_codes Output array
 * @param max_codes Maximum codes
 * @return Number of common codes
 */
uint32_t wiegand_get_common_facility_codes(WiegandManufacturer manufacturer,
                                           uint16_t* facility_codes, uint32_t max_codes);

// ========== Reader Configuration Helpers ==========

/**
 * Get default reader configuration
 * @param config Output configuration
 */
void wiegand_get_default_config(WiegandReaderConfig* config);

/**
 * Configure for HID readers
 * @param config Output configuration
 */
void wiegand_config_hid_reader(WiegandReaderConfig* config);

/**
 * Configure for AWID readers
 * @param config Output configuration
 */
void wiegand_config_awid_reader(WiegandReaderConfig* config);

/**
 * AUTHORIZATION WARNING:
 * 
 * Wiegand/SECURAKEY tools must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized building access
 * - Hospital/medical facility intrusion
 * - University credential fraud
 * - Hotel room access without authorization
 * - Government facility trespass
 * - Any form of unauthorized access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own access control systems
 * - Authorized penetration testing with written contracts
 * - Security research in controlled environments
 * - Educational purposes with proper oversight
 * - Backing up your own access cards
 * 
 * Unauthorized access control bypass is a FELONY in the United States.
 * 
 * Federal: Computer Fraud and Abuse Act (CFAA)
 * - Unauthorized access: Up to 10 years imprisonment
 * - Interstate commerce impact: Up to 20 years
 * 
 * State laws: Trespassing, burglary, identity theft charges
 * - Penalties vary but typically 1-5 years imprisonment + fines
 * 
 * Civil liability: Building owners can sue for damages
 * 
 * Hospitals/medical: HIPAA violations carry additional severe penalties
 */
