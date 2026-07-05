#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * BLE (Bluetooth Low Energy) Security Implementation
 * 
 * For authorized IoT and smart device security research.
 * Used in smart locks, wearables, IoT devices, home automation.
 * 
 * CRITICAL: Obtain written authorization before testing any BLE devices.
 */

// Forward declare app
struct PredatorApp;

// BLE pairing methods
typedef enum {
    BLEPairingJustWorks,        // No authentication (vulnerable)
    BLEPairingPasskey,          // 6-digit passkey
    BLEPairingNumericComparison, // Numeric comparison
    BLEPairingOutOfBand         // OOB authentication
} BLEPairingMethod;

// BLE security levels
typedef enum {
    BLESecurityLevelNone,       // No security
    BLESecurityLevelLow,        // Unauthenticated encryption
    BLESecurityLevelMedium,     // Authenticated encryption
    BLESecurityLevelHigh        // Authenticated LE Secure Connections
} BLESecurityLevel;

// BLE device info
typedef struct {
    uint8_t mac_address[6];     // BLE MAC address
    char name[32];              // Device name
    int8_t rssi;                // Signal strength
    BLEPairingMethod pairing;
    BLESecurityLevel security;
    uint8_t irk[16];            // Identity Resolving Key
    uint8_t ltk[16];            // Long Term Key
    bool keys_extracted;
} BLEDevice;

// Attack types
typedef enum {
    BLEAttackPasskeyBruteforce, // Brute force 6-digit PIN
    BLEAttackKNOB,              // Key Negotiation of Bluetooth
    BLEAttackBIAS,              // Bluetooth Impersonation AttackS
    BLEAttackPairingSniff,      // Sniff pairing process
    BLEAttackReplay,            // Replay attack
    BLEAttackDOWNGRADE          // Downgrade to legacy pairing
} BLEAttackType;

// Attack state
typedef struct {
    BLEAttackType attack_type;
    BLEDevice target;
    uint32_t passkeys_tried;
    uint32_t start_time;
    bool attack_running;
    bool pairing_sn iffed;
    uint8_t captured_packets[256][64]; // Captured BLE packets
    uint32_t packet_count;
} BLEAttackState;

// ========== BLE Crypto Functions ==========

/**
 * Generate BLE pairing confirm value
 * @param tk Temporary Key
 * @param rand Random number
 * @param confirm Output confirm value
 * @return true if successful
 */
bool ble_generate_confirm(const uint8_t* tk, const uint8_t* rand, uint8_t* confirm);

/**
 * Generate Short Term Key (STK)
 * @param tk Temporary Key
 * @param rand_initiator Initiator random
 * @param rand_responder Responder random
 * @param stk Output STK
 * @return true if successful
 */
bool ble_generate_stk(const uint8_t* tk, 
                      const uint8_t* rand_initiator,
                      const uint8_t* rand_responder,
                      uint8_t* stk);

/**
 * Generate Long Term Key (LTK)
 * @param stk Short Term Key
 * @param ediv Encrypted Diversifier
 * @param rand Random value
 * @param ltk Output LTK
 * @return true if successful
 */
bool ble_generate_ltk(const uint8_t* stk,
                      uint16_t ediv,
                      const uint8_t* rand,
                      uint8_t* ltk);

/**
 * AES-128 encryption for BLE
 * @param key Encryption key
 * @param plaintext Input data
 * @param ciphertext Output data
 * @return true if successful
 */
bool ble_aes128_encrypt(const uint8_t* key,
                        const uint8_t* plaintext,
                        uint8_t* ciphertext);

// ========== BLE Device Operations ==========

/**
 * Scan for BLE devices
 * @param app PredatorApp context
 * @param devices Output device array
 * @param max_devices Maximum devices to find
 * @param scan_duration_ms Scan duration
 * @return Number of devices found
 */
uint32_t ble_scan_devices(struct PredatorApp* app,
                          BLEDevice* devices,
                          uint32_t max_devices,
                          uint32_t scan_duration_ms);

/**
 * Connect to BLE device
 * @param app PredatorApp context
 * @param device Target device
 * @return true if connected
 */
bool ble_connect(struct PredatorApp* app, BLEDevice* device);

/**
 * Disconnect from BLE device
 * @param app PredatorApp context
 * @param device Connected device
 */
void ble_disconnect(struct PredatorApp* app, BLEDevice* device);

/**
 * Pair with BLE device
 * @param app PredatorApp context
 * @param device Target device
 * @param passkey Passkey (if required)
 * @return true if paired
 */
bool ble_pair(struct PredatorApp* app, BLEDevice* device, uint32_t passkey);

// ========== Attack Functions ==========

/**
 * Passkey brute force attack (6-digit PINs: 000000-999999)
 * @param app PredatorApp context
 * @param device Target device
 * @param found_passkey Output passkey if found
 * @return true if passkey found
 */
bool ble_attack_passkey_bruteforce(struct PredatorApp* app,
                                   BLEDevice* device,
                                   uint32_t* found_passkey);

/**
 * KNOB attack - force weak encryption
 * @param app PredatorApp context
 * @param device Target device
 * @return true if successful
 */
bool ble_attack_knob(struct PredatorApp* app, BLEDevice* device);

/**
 * BIAS attack - bypass authentication
 * @param app PredatorApp context
 * @param device Target device (previously paired)
 * @return true if successful
 */
bool ble_attack_bias(struct PredatorApp* app, BLEDevice* device);

/**
 * Sniff pairing process
 * @param app PredatorApp context
 * @param state Attack state with capture buffer
 * @return true if pairing captured
 */
bool ble_attack_sniff_pairing(struct PredatorApp* app, BLEAttackState* state);

/**
 * Extract keys from captured pairing
 * @param state Attack state with captured packets
 * @param device Output device with extracted keys
 * @return true if keys extracted
 */
bool ble_extract_keys_from_pairing(BLEAttackState* state, BLEDevice* device);

/**
 * Downgrade attack - force legacy pairing
 * @param app PredatorApp context
 * @param device Target device
 * @return true if downgrade successful
 */
bool ble_attack_downgrade(struct PredatorApp* app, BLEDevice* device);

// ========== Smart Lock Specific ==========

/**
 * Detect smart lock type
 * @param device BLE device
 * @return Smart lock manufacturer name
 */
const char* ble_detect_smart_lock(const BLEDevice* device);

/**
 * Test smart lock for common vulnerabilities
 * @param app PredatorApp context
 * @param device Smart lock device
 * @return Vulnerability count
 */
uint32_t ble_test_smart_lock_vulnerabilities(struct PredatorApp* app, BLEDevice* device);

// ========== Utilities ==========

/**
 * MAC address to string
 * @param mac MAC address bytes
 * @param str Output string buffer
 */
void ble_mac_to_string(const uint8_t* mac, char* str);

/**
 * Parse BLE advertisement data
 * @param adv_data Advertisement data
 * @param adv_len Data length
 * @param device Output device info
 * @return true if parsed
 */
bool ble_parse_advertisement(const uint8_t* adv_data, uint32_t adv_len, BLEDevice* device);

/**
 * Check if device is vulnerable to Just Works pairing
 * @param device BLE device
 * @return true if vulnerable
 */
bool ble_is_vulnerable_just_works(const BLEDevice* device);

// ========== Known Vulnerable Devices ==========

/**
 * Check if device is known vulnerable smart lock
 * @param device BLE device
 * @return true if known vulnerable
 */
bool ble_is_known_vulnerable_lock(const BLEDevice* device);

/**
 * Get known vulnerabilities for device
 * @param device BLE device
 * @param vulns Output vulnerability descriptions
 * @param max_vulns Maximum vulnerabilities
 * @return Number of vulnerabilities
 */
uint32_t ble_get_known_vulnerabilities(const BLEDevice* device,
                                       char vulns[][64],
                                       uint32_t max_vulns);

/**
 * AUTHORIZATION WARNING:
 * 
 * BLE security attacks must ONLY be used on devices you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Unauthorized smart lock access
 * - Breaking into homes or buildings
 * - Stealing or tampering with IoT devices
 * - Unauthorized surveillance or tracking
 * - Any form of trespassing or theft
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own smart devices
 * - Authorized IoT security research
 * - Professional penetration testing with contracts
 * - Educational research in controlled environments
 * - Vulnerability research with responsible disclosure
 * 
 * Unauthorized access via BLE attacks is a FELONY in most jurisdictions.
 * Smart lock attacks may violate computer fraud, burglary, and trespass laws.
 */
