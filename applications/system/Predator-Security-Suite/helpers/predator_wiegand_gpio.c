#include "predator_wiegand_gpio.h"
#include <furi_hal_gpio.h>
#include <furi.h>

// Global emulation state
static WiegandEmulationState* emulation_state = NULL;

/**
 * @brief Initialize Wiegand GPIO pins
 */
bool wiegand_gpio_init(PredatorApp* app) {
    UNUSED(app);
    
    // Configure D0 and D1 as outputs (open-drain with pull-up)
    furi_hal_gpio_init(WIEGAND_PIN_D0, GpioModeOutputOpenDrain, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_init(WIEGAND_PIN_D1, GpioModeOutputOpenDrain, GpioPullUp, GpioSpeedVeryHigh);
    
    // Set both lines high (idle state)
    furi_hal_gpio_write(WIEGAND_PIN_D0, true);
    furi_hal_gpio_write(WIEGAND_PIN_D1, true);
    
    FURI_LOG_I("WiegandGPIO", "GPIO initialized: D0=%p D1=%p", 
               (void*)WIEGAND_PIN_D0, (void*)WIEGAND_PIN_D1);
    
    return true;
}

/**
 * @brief Deinitialize Wiegand GPIO pins
 */
void wiegand_gpio_deinit(PredatorApp* app) {
    UNUSED(app);
    
    // Reset pins to input mode
    furi_hal_gpio_init(WIEGAND_PIN_D0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(WIEGAND_PIN_D1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    
    FURI_LOG_I("WiegandGPIO", "GPIO deinitialized");
}

/**
 * @brief Send single Wiegand bit
 */
void wiegand_send_bit(uint8_t bit) {
    const GpioPin* pin = (bit == 0) ? WIEGAND_PIN_D0 : WIEGAND_PIN_D1;
    
    // Pull line low for pulse
    furi_hal_gpio_write(pin, false);
    furi_delay_us(WIEGAND_PULSE_WIDTH_US);
    
    // Release line (pull-up returns it high)
    furi_hal_gpio_write(pin, true);
    furi_delay_us(WIEGAND_INTER_BIT_DELAY_US);
}

/**
 * @brief Transmit Wiegand card once
 */
bool wiegand_transmit_card(WiegandCard* card) {
    if(!card || card->bit_length == 0) {
        FURI_LOG_E("WiegandGPIO", "Invalid card data");
        return false;
    }
    
    FURI_LOG_D("WiegandGPIO", "Transmitting %u-bit card FC:%u Card:%lu",
               card->bit_length, card->facility_code, card->card_number);
    
    // Transmit each bit
    for(uint8_t i = 0; i < card->bit_length; i++) {
        uint8_t byte_index = i / 8;
        uint8_t bit_index = 7 - (i % 8);
        uint8_t bit = (card->raw_data[byte_index] >> bit_index) & 0x01;
        
        wiegand_send_bit(bit);
    }
    
    // Inter-card delay
    furi_delay_ms(WIEGAND_INTER_CARD_DELAY_MS);
    
    return true;
}

/**
 * @brief Emulation thread function
 */
static int32_t wiegand_emulation_thread(void* context) {
    UNUSED(context);
    
    if(!emulation_state) return -1;
    
    FURI_LOG_I("WiegandGPIO", "Emulation thread started");
    
    // Initialize GPIO
    wiegand_gpio_init(NULL);
    
    while(!emulation_state->should_stop) {
        // Lock mutex
        furi_mutex_acquire(emulation_state->mutex, FuriWaitForever);
        
        // Transmit card
        wiegand_transmit_card(&emulation_state->active_card);
        
        // Unlock mutex
        furi_mutex_release(emulation_state->mutex);
        
        // Small delay between repetitions
        furi_delay_ms(100);
    }
    
    // Cleanup GPIO
    wiegand_gpio_deinit(NULL);
    
    FURI_LOG_I("WiegandGPIO", "Emulation thread stopped");
    
    return 0;
}

/**
 * @brief Start continuous Wiegand emulation
 */
bool wiegand_start_emulation(PredatorApp* app, WiegandCard* card) {
    UNUSED(app);
    
    if(!card) {
        FURI_LOG_E("WiegandGPIO", "Cannot start emulation: null card");
        return false;
    }
    
    // Stop existing emulation if running
    if(emulation_state && emulation_state->is_emulating) {
        wiegand_stop_emulation(app);
    }
    
    // Allocate emulation state
    emulation_state = malloc(sizeof(WiegandEmulationState));
    if(!emulation_state) {
        FURI_LOG_E("WiegandGPIO", "Failed to allocate emulation state");
        return false;
    }
    
    memset(emulation_state, 0, sizeof(WiegandEmulationState));
    
    // Copy card data
    memcpy(&emulation_state->active_card, card, sizeof(WiegandCard));
    
    // Create mutex
    emulation_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!emulation_state->mutex) {
        FURI_LOG_E("WiegandGPIO", "Failed to create mutex");
        free(emulation_state);
        emulation_state = NULL;
        return false;
    }
    
    // Create emulation thread
    emulation_state->emulation_thread = furi_thread_alloc();
    furi_thread_set_name(emulation_state->emulation_thread, "WiegandEmul");
    furi_thread_set_stack_size(emulation_state->emulation_thread, 2048);
    furi_thread_set_callback(emulation_state->emulation_thread, wiegand_emulation_thread);
    furi_thread_set_context(emulation_state->emulation_thread, emulation_state);
    
    // Start thread
    emulation_state->should_stop = false;
    emulation_state->is_emulating = true;
    furi_thread_start(emulation_state->emulation_thread);
    
    FURI_LOG_I("WiegandGPIO", "Emulation started: FC:%u Card:%lu %u-bit",
               card->facility_code, card->card_number, card->bit_length);
    
    return true;
}

/**
 * @brief Stop Wiegand emulation
 */
void wiegand_stop_emulation(PredatorApp* app) {
    UNUSED(app);
    
    if(!emulation_state || !emulation_state->is_emulating) {
        return;
    }
    
    FURI_LOG_I("WiegandGPIO", "Stopping emulation...");
    
    // Signal thread to stop
    emulation_state->should_stop = true;
    emulation_state->is_emulating = false;
    
    // Wait for thread to finish
    if(emulation_state->emulation_thread) {
        furi_thread_join(emulation_state->emulation_thread);
        furi_thread_free(emulation_state->emulation_thread);
    }
    
    // Free mutex
    if(emulation_state->mutex) {
        furi_mutex_free(emulation_state->mutex);
    }
    
    // Free state
    free(emulation_state);
    emulation_state = NULL;
    
    FURI_LOG_I("WiegandGPIO", "Emulation stopped");
}

/**
 * @brief Try single card transmission
 */
bool wiegand_try_card(PredatorApp* app, WiegandCard* card) {
    UNUSED(app);
    
    if(!card) {
        return false;
    }
    
    // Initialize GPIO if not already done
    static bool gpio_initialized = false;
    if(!gpio_initialized) {
        wiegand_gpio_init(NULL);
        gpio_initialized = true;
    }
    
    // Transmit card once
    bool success = wiegand_transmit_card(card);
    
    if(success) {
        FURI_LOG_D("WiegandGPIO", "Card tried: FC:%u Card:%lu",
                   card->facility_code, card->card_number);
    }
    
    return success;
}

/**
 * @brief Get emulation state
 */
bool wiegand_is_emulating(PredatorApp* app) {
    UNUSED(app);
    return emulation_state && emulation_state->is_emulating;
}
