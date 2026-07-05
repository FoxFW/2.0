#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Megamos Crypto (DST40/DST80) Implementation
 * 
 * For authorized automotive security research.
 * Used in VW Group vehicles: VW, Audi, Porsche, Bentley, Lamborghini, Seat, Skoda.
 * Covers 30%+ of European automotive market.
 * 
 * CRITICAL: Obtain written authorization before testing any vehicle systems.
 */

// Forward declare app
struct PredatorApp;

// Megamos variants
typedef enum {
    MegamosDST40,    // 40-bit crypto (older VW/Audi)
    MegamosDST80,    // 80-bit crypto (modern VW Group)
    MegamosDST96     // 96-bit crypto (high-security)
} MegamosType;

// Transponder types
typedef enum {
    MegamosTransponder48,    // 48-bit ID
    MegamosTransponder96,    // 96-bit ID
    MegamosTransponderAES    // AES-based (newest)
} MegamosTransponderType;

// Key structure
typedef struct {
    MegamosType type;
    uint8_t key[12];         // Up to 96-bit key
    uint8_t key_len;         // 5 bytes (40-bit), 10 bytes (80-bit), 12 bytes (96-bit)
    uint32_t vehicle_id;     // Vehicle identification
} MegamosKey;

// Transponder data
typedef struct {
    MegamosTransponderType type;
    uint8_t id[12];          // Transponder ID (up to 96-bit)
    uint8_t id_len;
    MegamosKey key;
    bool authenticated;
} MegamosTransponder;

// Attack state
typedef struct {
    MegamosType target_type;
    MegamosTransponder transponder;
    uint64_t keys_tried;
    uint32_t start_time;
    bool attack_running;
} MegamosAttackState;

// ========== DST40 Cipher Functions ==========

/**
 * Initialize DST40 cipher
 * @param key 40-bit key (5 bytes)
 * @return Initial state
 */
uint64_t dst40_init(const uint8_t* key);

/**
 * DST40 encryption round
 * @param state Current cipher state
 * @param plaintext Input data
 * @return Encrypted output
 */
uint8_t dst40_encrypt(uint64_t* state, uint8_t plaintext);

/**
 * DST40 authentication
 * @param transponder_id Transponder ID
 * @param challenge Challenge from car
 * @param key Crypto key
 * @param response Output response
 * @return true if successful
 */
bool dst40_authenticate(const uint8_t* transponder_id, 
                        const uint8_t* challenge, 
                        const MegamosKey* key,
                        uint8_t* response);

// ========== DST80 Cipher Functions ==========

/**
 * Initialize DST80 cipher
 * @param key 80-bit key (10 bytes)
 * @return Initial state
 */
uint64_t dst80_init(const uint8_t* key);

/**
 * DST80 encryption round
 * @param state Current cipher state
 * @param plaintext Input data
 * @return Encrypted output
 */
uint8_t dst80_encrypt(uint64_t* state, uint8_t plaintext);

/**
 * DST80 authentication
 * @param transponder Transponder data
 * @param challenge Challenge from car
 * @param response Output response
 * @return true if successful
 */
bool dst80_authenticate(const MegamosTransponder* transponder,
                        const uint8_t* challenge,
                        uint8_t* response);

// ========== Transponder Operations ==========

/**
 * Detect Megamos transponder
 * @param app PredatorApp context
 * @param transponder Output transponder data
 * @return true if detected
 */
bool megamos_detect_transponder(struct PredatorApp* app, MegamosTransponder* transponder);

/**
 * Read transponder ID
 * @param app PredatorApp context
 * @param id Output ID buffer
 * @param max_len Maximum ID length
 * @return Actual ID length (0 on error)
 */
uint8_t megamos_read_id(struct PredatorApp* app, uint8_t* id, uint8_t max_len);

/**
 * Emulate transponder
 * @param app PredatorApp context
 * @param transponder Transponder to emulate
 * @return true if emulation started
 */
bool megamos_emulate(struct PredatorApp* app, const MegamosTransponder* transponder);

// ========== Attack Functions ==========

/**
 * Brute force attack (DST40 only - 40-bit keyspace)
 * @param app PredatorApp context
 * @param transponder Target transponder
 * @param found_key Output key if found
 * @return true if key found
 */
bool megamos_attack_bruteforce_dst40(struct PredatorApp* app,
                                     const MegamosTransponder* transponder,
                                     MegamosKey* found_key);

/**
 * Dictionary attack - try known keys
 * @param app PredatorApp context
 * @param transponder Target transponder
 * @param found_key Output key if found
 * @return true if key found
 */
bool megamos_attack_dictionary(struct PredatorApp* app,
                               const MegamosTransponder* transponder,
                               MegamosKey* found_key);

/**
 * Time-memory tradeoff attack
 * @param app PredatorApp context
 * @param transponder Target transponder
 * @param challenge Known challenge
 * @param response Known response
 * @param found_key Output key if found
 * @return true if key found
 */
bool megamos_attack_tmto(struct PredatorApp* app,
                         const MegamosTransponder* transponder,
                         const uint8_t* challenge,
                         const uint8_t* response,
                         MegamosKey* found_key);

/**
 * Side-channel attack (requires special hardware)
 * @param app PredatorApp context
 * @param transponder Target transponder
 * @param found_key Output key if found
 * @return true if key found
 */
bool megamos_attack_side_channel(struct PredatorApp* app,
                                 const MegamosTransponder* transponder,
                                 MegamosKey* found_key);

// ========== Manufacturer Detection ==========

/**
 * Detect vehicle manufacturer from transponder
 * @param transponder Transponder data
 * @return Manufacturer name string
 */
const char* megamos_detect_manufacturer(const MegamosTransponder* transponder);

/**
 * Get default keys for manufacturer
 * @param manufacturer Manufacturer name
 * @param keys Output key array
 * @param max_keys Maximum keys
 * @return Number of keys loaded
 */
uint32_t megamos_get_manufacturer_keys(const char* manufacturer,
                                       MegamosKey* keys,
                                       uint32_t max_keys);

// ========== Utilities ==========

/**
 * Calculate CRC for Megamos
 * @param data Data buffer
 * @param len Data length
 * @return CRC value
 */
uint16_t megamos_crc(const uint8_t* data, uint32_t len);

/**
 * Validate key
 * @param key Key to validate
 * @return true if valid
 */
bool megamos_validate_key(const MegamosKey* key);

/**
 * Key to string
 * @param key Key structure
 * @param str Output string buffer
 */
void megamos_key_to_string(const MegamosKey* key, char* str);

// ========== Vehicle Database ==========

// VW Group vehicle types
typedef enum {
    MegamosVehicleVW,
    MegamosVehicleAudi,
    MegamosVehiclePorsche,
    MegamosVehicleBentley,
    MegamosVehicleLamborghini,
    MegamosVehicleSeat,
    MegamosVehicleSkoda
} MegamosVehicleBrand;

/**
 * Get crypto type for vehicle
 * @param brand Vehicle brand
 * @param year Manufacturing year
 * @return Crypto type used
 */
MegamosType megamos_get_vehicle_crypto(MegamosVehicleBrand brand, uint16_t year);

/**
 * AUTHORIZATION WARNING:
 * 
 * Megamos Crypto attacks must ONLY be used on vehicles you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Vehicle theft or immobilizer bypass
 * - Unauthorized vehicle access
 * - Key cloning without authorization
 * - Any form of automotive crime
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own vehicle security
 * - Authorized automotive security research
 * - Professional locksmith services with proof of ownership
 * - Educational research in controlled environments
 * 
 * Unauthorized vehicle access is a FELONY in most jurisdictions.
 * Car theft laws carry severe penalties including imprisonment.
 */
