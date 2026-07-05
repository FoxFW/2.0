#include "predator_em4305_hal.h"
#include <furi_hal_rfid.h>
#include <toolbox/protocols/protocol.h>
#include <lfrfid/protocols/lfrfid_protocols.h>

/**
 * @brief Initialize EM4305 RFID hardware
 */
bool em4305_hal_init(PredatorApp* app) {
    UNUSED(app);
    
    // Initialize 125kHz RFID subsystem
    furi_hal_rfid_pins_reset();
    furi_hal_rfid_tim_reset();
    furi_hal_rfid_tim_read(125000, 0.5); // 125kHz carrier
    
    FURI_LOG_I("EM4305HAL", "RFID initialized at 125kHz");
    
    return true;
}

/**
 * @brief Deinitialize EM4305 RFID hardware
 */
void em4305_hal_deinit(PredatorApp* app) {
    UNUSED(app);
    
    // Stop RFID carrier
    furi_hal_rfid_tim_read_stop();
    furi_hal_rfid_pins_reset();
    
    FURI_LOG_I("EM4305HAL", "RFID deinitialized");
}

/**
 * @brief Detect card type from raw data
 */
EM4305CardType em4305_detect_card_type(const uint8_t* data, uint8_t length) {
    if(!data || length == 0) {
        return EM4305_Unknown;
    }
    
    // Check for EM4100 (64-bit pattern)
    if(length == 5 || length == 8) {
        // Verify EM4100 parity
        bool valid_parity = em4100_verify_parity(data);
        if(valid_parity) {
            return EM4305_EM4100;
        }
    }
    
    // Check for HID (26/35/37-bit patterns)
    if(length >= 3 && length <= 5) {
        // HID cards typically have specific header patterns
        if((data[0] & 0xF0) == 0x10 || (data[0] & 0xF0) == 0x20) {
            return EM4305_HID;
        }
    }
    
    // Check for Indala
    if(length >= 8) {
        // Indala has specific sync patterns
        if(data[0] == 0xAA || data[0] == 0x55) {
            return EM4305_Indala;
        }
    }
    
    return EM4305_Unknown;
}

/**
 * @brief Detect and read source card
 */
bool em4305_detect_source(PredatorApp* app, EM4305Card* card) {
    if(!card) {
        return false;
    }
    
    UNUSED(app);
    
    // Initialize RFID if not done
    static bool rfid_init = false;
    if(!rfid_init) {
        em4305_hal_init(NULL);
        rfid_init = true;
    }
    
    // Start reading
    furi_hal_rfid_tim_read_start();
    
    // Buffer for raw RFID data
    uint8_t raw_data[16];
    size_t raw_length = 0;
    
    // Try to read card (with timeout)
    uint32_t start_tick = furi_get_tick();
    bool card_detected = false;
    
    while((furi_get_tick() - start_tick) < EM4305_READ_TIMEOUT_MS) {
        // Read RFID data
        if(furi_hal_rfid_read(raw_data, sizeof(raw_data))) {
            raw_length = furi_hal_rfid_get_read_length();
            if(raw_length > 0) {
                card_detected = true;
                break;
            }
        }
        furi_delay_ms(10);
    }
    
    furi_hal_rfid_tim_read_stop();
    
    if(!card_detected) {
        FURI_LOG_D("EM4305HAL", "No card detected");
        return false;
    }
    
    // Detect card type
    card->card_type = em4305_detect_card_type(raw_data, raw_length);
    
    // Parse card data based on type
    switch(card->card_type) {
    case EM4305_EM4100:
        // Extract EM4100 ID (last 5 bytes)
        if(raw_length >= 5) {
            card->id = (raw_data[raw_length-5] << 24) |
                       (raw_data[raw_length-4] << 16) |
                       (raw_data[raw_length-3] << 8) |
                       (raw_data[raw_length-2]);
            card->id_length = 5;
            memcpy(card->data, raw_data, raw_length);
            
            FURI_LOG_I("EM4305HAL", "EM4100 detected: ID=%08lX", card->id);
            return true;
        }
        break;
        
    case EM4305_HID:
        // Parse HID card data
        if(raw_length >= 3) {
            memcpy(card->data, raw_data, raw_length);
            card->id_length = raw_length;
            
            // Extract facility and card number (basic parsing)
            card->id = (raw_data[0] << 16) | (raw_data[1] << 8) | raw_data[2];
            
            FURI_LOG_I("EM4305HAL", "HID detected: %02X%02X%02X...", 
                       raw_data[0], raw_data[1], raw_data[2]);
            return true;
        }
        break;
        
    case EM4305_Indala:
        // Parse Indala card
        if(raw_length >= 8) {
            memcpy(card->data, raw_data, raw_length);
            card->id_length = raw_length;
            card->id = (raw_data[4] << 24) | (raw_data[5] << 16) | 
                       (raw_data[6] << 8) | raw_data[7];
            
            FURI_LOG_I("EM4305HAL", "Indala detected");
            return true;
        }
        break;
        
    default:
        FURI_LOG_W("EM4305HAL", "Unknown card type");
        return false;
    }
    
    return false;
}

/**
 * @brief Detect blank EM4305 tag
 */
bool em4305_detect_blank(PredatorApp* app) {
    UNUSED(app);
    
    // Initialize RFID
    em4305_hal_init(NULL);
    
    // Start reading
    furi_hal_rfid_tim_read_start();
    
    // Try to detect T55xx/EM4305 response
    uint8_t test_data[4];
    bool detected = false;
    
    uint32_t start_tick = furi_get_tick();
    while((furi_get_tick() - start_tick) < EM4305_READ_TIMEOUT_MS) {
        // Try to read block 0 (configuration)
        if(furi_hal_rfid_read(test_data, sizeof(test_data))) {
            // Check if we get a valid response
            detected = true;
            break;
        }
        furi_delay_ms(10);
    }
    
    furi_hal_rfid_tim_read_stop();
    
    if(detected) {
        FURI_LOG_I("EM4305HAL", "Blank EM4305 detected");
    }
    
    return detected;
}

/**
 * @brief Write single block to EM4305
 */
bool em4305_write_block(PredatorApp* app, uint8_t block, EM4305Card* card) {
    if(!card || block > 4) {
        FURI_LOG_E("EM4305HAL", "Invalid block number: %u", block);
        return false;
    }
    
    UNUSED(app);
    
    // Calculate block offset in card data
    uint8_t offset = (block - 1) * EM4305_BLOCK_SIZE;
    if(offset + EM4305_BLOCK_SIZE > sizeof(card->data)) {
        return false;
    }
    
    // Get block data
    uint32_t block_data = (card->data[offset] << 24) |
                          (card->data[offset+1] << 16) |
                          (card->data[offset+2] << 8) |
                          (card->data[offset+3]);
    
    FURI_LOG_D("EM4305HAL", "Writing block %u: %08lX", block, block_data);
    
    // Start write mode
    furi_hal_rfid_tim_read_start();
    
    // Send write command (T55xx protocol)
    // Format: [Opcode][Password][Block][Data]
    bool success = furi_hal_rfid_write_block(block, block_data, EM4305_DEFAULT_PASSWORD);
    
    furi_hal_rfid_tim_read_stop();
    
    if(success) {
        FURI_LOG_I("EM4305HAL", "Block %u written successfully", block);
    } else {
        FURI_LOG_E("EM4305HAL", "Failed to write block %u", block);
    }
    
    // Delay for write to complete
    furi_delay_ms(50);
    
    return success;
}

/**
 * @brief Read entire EM4305 card
 */
bool em4305_read_card(PredatorApp* app, EM4305Card* card) {
    if(!card) {
        return false;
    }
    
    UNUSED(app);
    
    // Initialize
    em4305_hal_init(NULL);
    furi_hal_rfid_tim_read_start();
    
    // Read all blocks (1-4)
    bool success = true;
    for(uint8_t block = 1; block <= 4; block++) {
        uint32_t block_data = 0;
        
        if(!furi_hal_rfid_read_block(block, &block_data)) {
            FURI_LOG_W("EM4305HAL", "Failed to read block %u", block);
            success = false;
            continue;
        }
        
        // Store block data
        uint8_t offset = (block - 1) * EM4305_BLOCK_SIZE;
        card->data[offset] = (block_data >> 24) & 0xFF;
        card->data[offset+1] = (block_data >> 16) & 0xFF;
        card->data[offset+2] = (block_data >> 8) & 0xFF;
        card->data[offset+3] = block_data & 0xFF;
        
        FURI_LOG_D("EM4305HAL", "Block %u: %08lX", block, block_data);
    }
    
    furi_hal_rfid_tim_read_stop();
    
    if(success) {
        // Extract ID from data
        card->id = (card->data[0] << 24) | (card->data[1] << 16) |
                   (card->data[2] << 8) | card->data[3];
        card->id_length = 16; // 4 blocks * 4 bytes
        
        FURI_LOG_I("EM4305HAL", "Card read: ID=%08lX", card->id);
    }
    
    return success;
}

/**
 * @brief Write full card data to EM4305
 */
bool em4305_write_card(PredatorApp* app, EM4305Card* card) {
    if(!card) {
        return false;
    }
    
    FURI_LOG_I("EM4305HAL", "Writing full card...");
    
    // Write all blocks
    bool success = true;
    for(uint8_t block = 1; block <= 4; block++) {
        if(!em4305_write_block(app, block, card)) {
            FURI_LOG_E("EM4305HAL", "Failed to write block %u", block);
            success = false;
        }
        furi_delay_ms(50); // Delay between writes
    }
    
    if(success) {
        FURI_LOG_I("EM4305HAL", "Card written successfully");
    }
    
    return success;
}

/**
 * @brief Authenticate with password
 */
bool em4305_authenticate(PredatorApp* app, uint32_t password) {
    UNUSED(app);
    
    FURI_LOG_D("EM4305HAL", "Authenticating with password: %08lX", password);
    
    // Send password command
    furi_hal_rfid_tim_read_start();
    
    // T55xx password command
    bool success = furi_hal_rfid_write_password(password);
    
    furi_hal_rfid_tim_read_stop();
    
    if(success) {
        FURI_LOG_I("EM4305HAL", "Authentication successful");
    } else {
        FURI_LOG_W("EM4305HAL", "Authentication failed");
    }
    
    return success;
}

/**
 * @brief Configure EM4305 for EM4100 emulation
 */
bool em4305_configure_em4100(PredatorApp* app, EM4305Card* card) {
    if(!card) {
        return false;
    }
    
    FURI_LOG_I("EM4305HAL", "Configuring EM4305 for EM4100 emulation");
    
    // EM4100 configuration for block 0
    // Manchester encoding, 64-bit RF/64, max block 2
    uint32_t config = 0x00148040;
    
    // Write configuration
    furi_hal_rfid_tim_read_start();
    bool success = furi_hal_rfid_write_block(0, config, EM4305_DEFAULT_PASSWORD);
    furi_hal_rfid_tim_read_stop();
    
    if(!success) {
        FURI_LOG_E("EM4305HAL", "Failed to write configuration");
        return false;
    }
    
    furi_delay_ms(50);
    
    // Write EM4100 data
    return em4305_write_card(app, card);
}

/**
 * @brief Configure EM4305 for HID Prox emulation
 */
bool em4305_configure_hid(PredatorApp* app, EM4305Card* card) {
    if(!card) {
        return false;
    }
    
    FURI_LOG_I("EM4305HAL", "Configuring EM4305 for HID emulation");
    
    // HID configuration for block 0
    // FSK2a, RF/50, max block 2
    uint32_t config = 0x00107060;
    
    // Write configuration
    furi_hal_rfid_tim_read_start();
    bool success = furi_hal_rfid_write_block(0, config, EM4305_DEFAULT_PASSWORD);
    furi_hal_rfid_tim_read_stop();
    
    if(!success) {
        FURI_LOG_E("EM4305HAL", "Failed to write configuration");
        return false;
    }
    
    furi_delay_ms(50);
    
    // Write HID data
    return em4305_write_card(app, card);
}
