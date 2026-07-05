#pragma once

#include "../predator_i.h"
#include "predator_crypto_securakey.h"
#include <furi_hal_gpio.h>

/**
 * @brief Wiegand GPIO Hardware Abstraction Layer
 * 
 * Production-ready implementation for Wiegand protocol over GPIO
 * Standard Wiegand timing: 50µs pulse width, 1ms inter-bit delay
 * 
 * GPIO Pins:
 * - D0: GPIO 5 (PC5) - Data 0 line (even parity)
 * - D1: GPIO 6 (PC6) - Data 1 line (odd parity)
 * 
 * @note Requires authorization for security testing
 */

// GPIO pin definitions for Wiegand
#define WIEGAND_PIN_D0 &gpio_ext_pc5  // Data 0 line
#define WIEGAND_PIN_D1 &gpio_ext_pc6  // Data 1 line

// Wiegand timing constants (microseconds)
#define WIEGAND_PULSE_WIDTH_US 50      // Standard pulse width
#define WIEGAND_INTER_BIT_DELAY_US 1000 // 1ms between bits
#define WIEGAND_INTER_CARD_DELAY_MS 50  // 50ms between card transmissions

// Wiegand emulation state
typedef struct {
    bool is_emulating;
    WiegandCard active_card;
    FuriThread* emulation_thread;
    FuriMutex* mutex;
    volatile bool should_stop;
} WiegandEmulationState;

/**
 * @brief Initialize Wiegand GPIO pins
 * @param app Application context
 * @return true on success
 */
bool wiegand_gpio_init(PredatorApp* app);

/**
 * @brief Deinitialize Wiegand GPIO pins
 * @param app Application context
 */
void wiegand_gpio_deinit(PredatorApp* app);

/**
 * @brief Start continuous Wiegand card emulation
 * @param app Application context
 * @param card Card to emulate
 * @return true on success
 */
bool wiegand_start_emulation(PredatorApp* app, WiegandCard* card);

/**
 * @brief Stop Wiegand emulation
 * @param app Application context
 */
void wiegand_stop_emulation(PredatorApp* app);

/**
 * @brief Try single card transmission
 * @param app Application context
 * @param card Card to try
 * @return true if transmission successful
 */
bool wiegand_try_card(PredatorApp* app, WiegandCard* card);

/**
 * @brief Transmit Wiegand card once
 * @param card Card data to transmit
 * @return true on success
 */
bool wiegand_transmit_card(WiegandCard* card);

/**
 * @brief Send single Wiegand bit
 * @param bit Bit value (0 or 1)
 */
void wiegand_send_bit(uint8_t bit);

/**
 * @brief Get emulation state
 * @param app Application context
 * @return true if currently emulating
 */
bool wiegand_is_emulating(PredatorApp* app);
