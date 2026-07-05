#pragma once

#include "../predator_i.h"
#include "predator_crypto_em4305.h"
#include <furi_hal_rfid.h>

/**
 * @brief EM4305 Hardware Abstraction Layer
 * 
 * Production-ready implementation for EM4305/EM4100/HID 125kHz RFID
 * 
 * Supported Operations:
 * - Detect and read EM4100/HID Prox/Indala cards
 * - Detect blank EM4305 tags
 * - Write data to EM4305 (T55xx protocol)
 * - Read EM4305 card data
 * - Password authentication
 * 
 * @note Requires 125kHz RFID hardware support
 */

// RFID timing constants (microseconds)
#define EM4305_READ_TIMEOUT_MS 500
#define EM4305_WRITE_TIMEOUT_MS 1000
#define EM4305_PASSWORD_TIMEOUT_MS 200

// EM4305 configuration
#define EM4305_BLOCKS_COUNT 5       // Blocks 1-4 are user data
#define EM4305_BLOCK_SIZE 4         // 4 bytes per block
#define EM4305_DEFAULT_PASSWORD 0x00000000

// T55xx commands (for EM4305 compatibility)
#define T55XX_CMD_READ_BLOCK 0x00
#define T55XX_CMD_WRITE_BLOCK 0x01
#define T55XX_CMD_PASSWORD 0x02

/**
 * @brief Initialize EM4305 RFID hardware
 * @param app Application context
 * @return true on success
 */
bool em4305_hal_init(PredatorApp* app);

/**
 * @brief Deinitialize EM4305 RFID hardware
 * @param app Application context
 */
void em4305_hal_deinit(PredatorApp* app);

/**
 * @brief Detect and read source card (EM4100/HID/Indala)
 * @param app Application context
 * @param card Output card data
 * @return true if card detected and read
 */
bool em4305_detect_source(PredatorApp* app, EM4305Card* card);

/**
 * @brief Detect blank EM4305 tag
 * @param app Application context
 * @return true if blank EM4305 detected
 */
bool em4305_detect_blank(PredatorApp* app);

/**
 * @brief Write single block to EM4305
 * @param app Application context
 * @param block Block number (1-4)
 * @param card Card with data to write
 * @return true on success
 */
bool em4305_write_block(PredatorApp* app, uint8_t block, EM4305Card* card);

/**
 * @brief Read entire EM4305 card
 * @param app Application context
 * @param card Output card data
 * @return true on success
 */
bool em4305_read_card(PredatorApp* app, EM4305Card* card);

/**
 * @brief Write full card data to EM4305
 * @param app Application context
 * @param card Card data to write
 * @return true on success
 */
bool em4305_write_card(PredatorApp* app, EM4305Card* card);

/**
 * @brief Authenticate with password
 * @param app Application context
 * @param password 32-bit password
 * @return true if authenticated
 */
bool em4305_authenticate(PredatorApp* app, uint32_t password);

/**
 * @brief Detect card type (EM4100, HID, Indala)
 * @param data Raw card data
 * @param length Data length
 * @return Card type
 */
EM4305CardType em4305_detect_card_type(const uint8_t* data, uint8_t length);

/**
 * @brief Configure EM4305 for EM4100 emulation
 * @param app Application context
 * @param card EM4100 card data
 * @return true on success
 */
bool em4305_configure_em4100(PredatorApp* app, EM4305Card* card);

/**
 * @brief Configure EM4305 for HID Prox emulation
 * @param app Application context
 * @param card HID card data
 * @return true on success
 */
bool em4305_configure_hid(PredatorApp* app, EM4305Card* card);
