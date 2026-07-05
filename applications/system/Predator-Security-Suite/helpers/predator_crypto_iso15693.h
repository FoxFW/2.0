#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * ISO 15693 (Vicinity Cards) Implementation
 * 
 * Long-range RFID standard (up to 1.5m vs 10cm for MIFARE)
 * Used in: Libraries, supply chain, pharmaceutical tracking, industrial access
 * 
 * Common tags: Ti Tag-it, NXP ICODE SLI/SLIX/SLIX2, Infineon my-d
 * 
 * CRITICAL: Obtain written authorization before testing any ISO 15693 systems.
 */

// Forward declare app
struct PredatorApp;

// ISO 15693 tag types (detected from manufacturer code)
typedef enum {
    ISO15693_Unknown,
    ISO15693_TI_TagIt,           // Texas Instruments Tag-it
    ISO15693_NXP_ICODE_SLI,      // NXP ICODE SLI (no password)
    ISO15693_NXP_ICODE_SLIX,     // NXP ICODE SLIX (password protected)
    ISO15693_NXP_ICODE_SLIX2,    // NXP ICODE SLIX2 (advanced security)
    ISO15693_Infineon_myD,       // Infineon my-d
    ISO15693_STM_LRI,            // STMicroelectronics LRI series
    ISO15693_Fujitsu_MB89R118    // Fujitsu MB89R series
} ISO15693TagType;

// Security features
typedef enum {
    ISO15693_SecurityNone,       // No security (most tags)
    ISO15693_SecurityPassword,   // Password protection (SLIX)
    ISO15693_SecurityPrivacy,    // Privacy mode (SLIX2)
    ISO15693_SecurityEAS         // Electronic Article Surveillance
} ISO15693Security;

// Tag information structure
typedef struct {
    uint8_t uid[8];              // 64-bit unique identifier (reverse order!)
    uint8_t dsfid;               // Data Storage Format Identifier
    uint8_t afi;                 // Application Family Identifier
    uint16_t block_count;        // Number of memory blocks
    uint8_t block_size;          // Block size in bytes (usually 4)
    uint16_t ic_reference;       // IC manufacturer reference
    ISO15693TagType tag_type;
    ISO15693Security security;
    bool eas_enabled;            // Electronic Article Surveillance flag
    bool password_protected;
    uint32_t password;           // For SLIX tags
} ISO15693Tag;

// Command flags (used in all commands)
#define ISO15693_FLAG_SUBCARRIER_SINGLE   0x00
#define ISO15693_FLAG_SUBCARRIER_DUAL     0x01
#define ISO15693_FLAG_DATA_RATE_HIGH      0x02
#define ISO15693_FLAG_INVENTORY           0x04
#define ISO15693_FLAG_PROTOCOL_EXT        0x08
#define ISO15693_FLAG_SELECT              0x10
#define ISO15693_FLAG_ADDRESS             0x20
#define ISO15693_FLAG_OPTION              0x40

// ========== Detection & Identification ==========

/**
 * Detect ISO 15693 tag
 * @param app PredatorApp context
 * @param tag Output tag structure
 * @return true if tag detected
 */
bool iso15693_detect_tag(struct PredatorApp* app, ISO15693Tag* tag);

/**
 * Get system information (block count, size, IC ref)
 * @param app PredatorApp context
 * @param tag Tag structure to populate
 * @return true if successful
 */
bool iso15693_get_system_info(struct PredatorApp* app, ISO15693Tag* tag);

/**
 * Inventory (scan for multiple tags)
 * @param app PredatorApp context
 * @param uids Output UID array
 * @param max_tags Maximum tags to find
 * @param afi Optional AFI filter (0 = no filter)
 * @return Number of tags found
 */
uint32_t iso15693_inventory(struct PredatorApp* app, uint8_t uids[][8], 
                            uint32_t max_tags, uint8_t afi);

/**
 * Read tag UID
 * @param app PredatorApp context
 * @param uid Output UID buffer (8 bytes)
 * @return true if successful
 */
bool iso15693_read_uid(struct PredatorApp* app, uint8_t* uid);

// ========== Memory Operations ==========

/**
 * Read single block
 * @param app PredatorApp context
 * @param tag Tag structure with UID
 * @param block_number Block number to read
 * @param data Output buffer (block_size bytes)
 * @return true if successful
 */
bool iso15693_read_block(struct PredatorApp* app, const ISO15693Tag* tag,
                         uint16_t block_number, uint8_t* data);

/**
 * Read multiple blocks
 * @param app PredatorApp context
 * @param tag Tag structure with UID
 * @param start_block First block to read
 * @param block_count Number of blocks
 * @param data Output buffer (block_count * block_size bytes)
 * @return Number of blocks read
 */
uint32_t iso15693_read_blocks(struct PredatorApp* app, const ISO15693Tag* tag,
                              uint16_t start_block, uint16_t block_count,
                              uint8_t* data);

/**
 * Write single block
 * @param app PredatorApp context
 * @param tag Tag structure with UID
 * @param block_number Block number to write
 * @param data Data to write (block_size bytes)
 * @return true if successful
 */
bool iso15693_write_block(struct PredatorApp* app, const ISO15693Tag* tag,
                          uint16_t block_number, const uint8_t* data);

/**
 * Write multiple blocks
 * @param app PredatorApp context
 * @param tag Tag structure with UID
 * @param start_block First block to write
 * @param block_count Number of blocks
 * @param data Data to write (block_count * block_size bytes)
 * @return Number of blocks written
 */
uint32_t iso15693_write_blocks(struct PredatorApp* app, const ISO15693Tag* tag,
                               uint16_t start_block, uint16_t block_count,
                               const uint8_t* data);

/**
 * Lock block (permanent - cannot be undone!)
 * @param app PredatorApp context
 * @param tag Tag structure with UID
 * @param block_number Block to lock
 * @return true if successful
 */
bool iso15693_lock_block(struct PredatorApp* app, const ISO15693Tag* tag,
                        uint16_t block_number);

// ========== Security Operations (SLIX/SLIX2) ==========

/**
 * Get random number (for password challenge)
 * @param app PredatorApp context
 * @param random Output random number (2 bytes)
 * @return true if successful
 */
bool iso15693_get_random_number(struct PredatorApp* app, uint8_t* random);

/**
 * Set password (SLIX tags)
 * @param app PredatorApp context
 * @param tag Tag structure
 * @param password_id Password identifier (read=1, write=2, privacy=4, destroy=8)
 * @param password 32-bit password
 * @return true if successful
 */
bool iso15693_set_password(struct PredatorApp* app, ISO15693Tag* tag,
                           uint8_t password_id, uint32_t password);

/**
 * Verify password (unlock tag)
 * @param app PredatorApp context
 * @param tag Tag structure
 * @param password_id Password identifier
 * @param password 32-bit password
 * @return true if password correct
 */
bool iso15693_verify_password(struct PredatorApp* app, const ISO15693Tag* tag,
                              uint8_t password_id, uint32_t password);

/**
 * Enable privacy mode (hides UID until password provided)
 * @param app PredatorApp context
 * @param tag Tag structure
 * @return true if successful
 */
bool iso15693_enable_privacy(struct PredatorApp* app, const ISO15693Tag* tag);

/**
 * Disable privacy mode
 * @param app PredatorApp context
 * @param password Privacy password
 * @return true if successful
 */
bool iso15693_disable_privacy(struct PredatorApp* app, uint32_t password);

// ========== EAS (Electronic Article Surveillance) ==========

/**
 * Read EAS status
 * @param app PredatorApp context
 * @param tag Tag structure
 * @param eas_enabled Output EAS flag
 * @return true if successful
 */
bool iso15693_read_eas_status(struct PredatorApp* app, const ISO15693Tag* tag,
                              bool* eas_enabled);

/**
 * Set EAS (activate anti-theft alarm)
 * @param app PredatorApp context
 * @param tag Tag structure
 * @return true if successful
 */
bool iso15693_set_eas(struct PredatorApp* app, const ISO15693Tag* tag);

/**
 * Reset EAS (deactivate anti-theft alarm)
 * @param app PredatorApp context
 * @param tag Tag structure
 * @return true if successful
 */
bool iso15693_reset_eas(struct PredatorApp* app, const ISO15693Tag* tag);

// ========== Attack Functions ==========

/**
 * Dictionary attack on password-protected tags
 * @param app PredatorApp context
 * @param tag Tag structure
 * @param password_id Password identifier to attack
 * @param found_password Output password if found
 * @return true if password found
 */
bool iso15693_attack_password(struct PredatorApp* app, const ISO15693Tag* tag,
                              uint8_t password_id, uint32_t* found_password);

/**
 * Brute force password (32-bit keyspace - feasible)
 * @param app PredatorApp context
 * @param tag Tag structure
 * @param password_id Password identifier
 * @param start_password Starting password for brute force
 * @param end_password Ending password
 * @param found_password Output password if found
 * @return true if password found
 */
bool iso15693_bruteforce_password(struct PredatorApp* app, const ISO15693Tag* tag,
                                  uint8_t password_id, uint32_t start_password,
                                  uint32_t end_password, uint32_t* found_password);

/**
 * Dump entire tag memory
 * @param app PredatorApp context
 * @param tag Tag structure
 * @param output_path Path to save dump
 * @return true if successful
 */
bool iso15693_dump_tag(struct PredatorApp* app, const ISO15693Tag* tag,
                      const char* output_path);

/**
 * Clone tag (copy all blocks to another tag)
 * @param app PredatorApp context
 * @param source Source tag
 * @param target Target writable tag
 * @return true if successful
 */
bool iso15693_clone_tag(struct PredatorApp* app, const ISO15693Tag* source,
                       ISO15693Tag* target);

// ========== Utilities ==========

/**
 * Calculate CRC16 for ISO 15693
 * @param data Data buffer
 * @param len Data length
 * @return CRC16 value
 */
uint16_t iso15693_crc16(const uint8_t* data, uint32_t len);

/**
 * Reverse UID byte order (ISO 15693 uses LSB first)
 * @param uid UID to reverse (8 bytes)
 */
void iso15693_reverse_uid(uint8_t* uid);

/**
 * Detect tag type from manufacturer code and IC reference
 * @param uid UID (manufacturer code is first byte)
 * @param ic_ref IC reference from system info
 * @return Detected tag type
 */
ISO15693TagType iso15693_detect_type(const uint8_t* uid, uint16_t ic_ref);

/**
 * Get tag type name
 * @param type Tag type
 * @return Human-readable name
 */
const char* iso15693_get_type_name(ISO15693TagType type);

/**
 * Convert AFI to application name
 * @param afi Application Family Identifier
 * @return Application name
 */
const char* iso15693_afi_to_string(uint8_t afi);

// ========== Common Default Passwords ==========

// NXP ICODE SLIX default passwords (often unchanged)
extern const uint32_t ISO15693_PASSWORD_DEFAULT;
extern const uint32_t ISO15693_PASSWORD_NXP_SAMPLE;
extern const uint32_t ISO15693_PASSWORD_LIBRARY_COMMON[];
extern const uint32_t ISO15693_PASSWORD_PHARMA_COMMON[];

/**
 * Load common passwords for dictionary attack
 * @param passwords Output password array
 * @param max_passwords Maximum passwords
 * @return Number of passwords loaded
 */
uint32_t iso15693_load_common_passwords(uint32_t* passwords, uint32_t max_passwords);

/**
 * AUTHORIZATION WARNING:
 * 
 * ISO 15693 attacks must ONLY be used on systems you own or have
 * explicit written authorization to test.
 * 
 * ILLEGAL USE CASES (DO NOT):
 * - Library book theft or system manipulation
 * - Pharmaceutical supply chain fraud
 * - Industrial access control bypass
 * - Anti-theft system manipulation
 * - Any form of unauthorized access
 * 
 * LEGAL USE CASES (AUTHORIZED ONLY):
 * - Testing your own ISO 15693 systems
 * - Authorized penetration testing with contracts
 * - Security research in controlled environments
 * - Educational purposes with proper oversight
 * 
 * Unauthorized RFID manipulation is a FELONY in most jurisdictions.
 * May violate computer fraud, theft, and supply chain integrity laws.
 */
