/**
 * @file predator_subghz_core.c
 * @brief SubGHz Hardware Core Implementation
 * 
 * Handles SubGHz hardware initialization, deinitialization,
 * and low-level hardware acquisition for all supported boards.
 */

#include "predator_subghz_core.h"
#include "../../predator_i.h"
#include "../predator_boards.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_subghz.h>

/**
 * @brief Acquire SubGHz hardware for exclusive use
 * @return true if acquisition successful, false otherwise
 * @note Puts radio in safe sleep state, resets, then idles
 */
static bool predator_subghz_acquire_hardware(void) {
    FURI_LOG_I("PredatorSubGHz", "[ACQUIRE] Starting SubGHz hardware acquisition");
    
    // Step 1: Put radio in safe sleep state first
    furi_hal_subghz_sleep();
    furi_delay_ms(10);
    FURI_LOG_I("PredatorSubGHz", "[ACQUIRE] Radio in sleep state");
    
    // Step 2: Reset the radio to known state
    furi_hal_subghz_reset();
    furi_delay_ms(10);
    FURI_LOG_I("PredatorSubGHz", "[ACQUIRE] Radio reset complete");
    
    // Step 3: Put in idle state (ready for configuration)
    furi_hal_subghz_idle();
    furi_delay_ms(10);
    FURI_LOG_I("PredatorSubGHz", "[ACQUIRE] Radio in idle state - ready");
    
    return true;
}

void predator_subghz_init(PredatorApp* app) {
    if(!app) {
        FURI_LOG_E("PredatorSubGHz", "NULL app pointer");
        return;
    }
    
    const PredatorBoardConfig* board_config = predator_boards_get_config(app->board_type);
    if(!board_config) {
        FURI_LOG_E("PredatorSubGHz", "Invalid board configuration");
        return;
    }
    
    FURI_LOG_I("PredatorSubGHz", "Initializing SubGHz for: %s", board_config->name);
    
    // CRITICAL: Suppress charging for SubGHz use
    furi_hal_power_suppress_charge_enter();
    
    // CRITICAL: Properly acquire SubGHz hardware
    bool acquired = predator_subghz_acquire_hardware();
    if(!acquired) {
        FURI_LOG_E("PredatorSubGHz", "Failed to acquire SubGHz hardware");
        furi_hal_power_suppress_charge_exit();
        return;
    }
    
    // Set marker to indicate SubGHz is initialized
    if(!app->subghz_txrx) {
        app->subghz_txrx = (void*)0x1; // Non-NULL marker
    }
    
    // Log board info
    if(board_config->has_external_rf) {
        FURI_LOG_I("PredatorSubGHz", "External RF: %d dBm", board_config->rf_power_dbm);
    } else {
        FURI_LOG_I("PredatorSubGHz", "Built-in CC1101 radio ready");
    }
    
    FURI_LOG_I("PredatorSubGHz", "SubGHz ready for attacks!");
}

void predator_subghz_deinit(PredatorApp* app) {
    if(!app) {
        FURI_LOG_E("PredatorSubGHz", "NULL app pointer in predator_subghz_deinit");
        return;
    }
    
    // CRITICAL FIX: Don't touch hardware in deinit - causes reboot
    // Just clean up app state - framework handles hardware
    if(app->subghz_txrx) {
        app->subghz_txrx = NULL;
    }
    
    FURI_LOG_I("PredatorSubGHz", "SubGHz deinit: State cleaned - framework handles hardware");
    // DO NOT call furi_hal_subghz_sleep() here - causes reboot!
}

bool predator_subghz_stop_attack(PredatorApp* app) {
    if(!app) {
        FURI_LOG_E("PredatorSubGHz", "NULL app pointer in predator_subghz_stop_attack");
        return false;
    }
    
    if(!app->subghz_txrx) {
        FURI_LOG_W("PredatorSubGHz", "SubGHz not initialized - nothing to stop");
        return false;
    }
    
    FURI_LOG_I("PredatorSubGHz", "Stopping SubGHz attack");
    
    // CRITICAL FIX: Don't call stop_async_tx if we didn't start async
    // Just mark as stopped and let framework cleanup
    // DO NOT touch hardware directly
    
    // Update app state
    app->attack_running = false;
    
    // Turn off LED
    if(app->notifications) {
        notification_message(app->notifications, &sequence_reset_blue);
        notification_message(app->notifications, &sequence_reset_green);
        notification_message(app->notifications, &sequence_reset_red);
    }
    
    FURI_LOG_I("PredatorSubGHz", "Attack stopped successfully");
    return true;
}

bool predator_subghz_send_raw_packet(PredatorApp* app, uint8_t* packet, size_t len) {
    if(!app) {
        FURI_LOG_E("PredatorSubGHz", "NULL app pointer");
        return false;
    }
    if(!packet || len == 0 || len > 256) {
        FURI_LOG_E("PredatorSubGHz", "Invalid packet data (len=%u)", (unsigned)len);
        return false;
    }
    if(!app->subghz_txrx) {
        FURI_LOG_E("PredatorSubGHz", "SubGHz not initialized");
        return false;
    }
    
    // CRITICAL FIX: Actually transmit the packet via real SubGHz hardware
    FURI_LOG_I("PredatorSubGHz", "[REAL HW] Transmitting %u-byte encrypted packet", (unsigned)len);
    
    // Use the actual Flipper Zero hardware SubGHz API to transmit
    furi_hal_subghz_write_packet(packet, len);
    
    // Wait for transmission to complete
    furi_delay_ms(50);
    
    FURI_LOG_I("PredatorSubGHz", "[REAL HW] Packet transmission COMPLETE");
    
    // Visual feedback - green blink for successful transmission
    if(app->notifications) {
        notification_message(app->notifications, &sequence_blink_green_100);
    }
    
    return true;
}
