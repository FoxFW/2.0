#include "predator_watchdog.h"
#include "predator_error.h"
#include <furi.h>

typedef struct {
    FuriTimer* timer;
    uint32_t timeout_ms;
    uint32_t last_kick;
    bool active;
} PredatorWatchdog;

// Static watchdog instance for monitoring
static PredatorWatchdog* watchdog = NULL;

static void watchdog_timer_callback(void* context) {
    PredatorApp* app = (PredatorApp*)context;
    
    if(!app || !watchdog) return;
    
    // Check if timeout occurred
    uint32_t now = furi_get_tick();
    if((now - watchdog->last_kick) > watchdog->timeout_ms) {
        // Timeout occurred - handle the error
        predator_error_handle(
            app, 
            PredatorErrorTimeout, 
            "Operation timed out - watchdog triggered recovery", 
            true);
            
        // Attempt recovery
        // In a real app, you might reset certain subsystems or go back to a safe state
    }
}

bool predator_watchdog_init(PredatorApp* app) {
    if(!app) return false;
    
    // Only initialize once
    if(watchdog) return true;
    
    // Allocate watchdog
    watchdog = malloc(sizeof(PredatorWatchdog));
    if(!watchdog) {
        FURI_LOG_E("Predator", "Failed to allocate watchdog");
        return false;
    }
    
    // Clear struct
    memset(watchdog, 0, sizeof(PredatorWatchdog));
    
    // Create timer
    watchdog->timer = furi_timer_alloc(watchdog_timer_callback, FuriTimerTypePeriodic, app);
    if(!watchdog->timer) {
        FURI_LOG_E("Predator", "Failed to allocate watchdog timer");
        free(watchdog);
        watchdog = NULL;
        return false;
    }
    
    return true;
}

bool predator_watchdog_start(PredatorApp* app, uint32_t timeout_ms) {
    if(!app || !watchdog || !watchdog->timer) return false;
    
    // Set timeout
    watchdog->timeout_ms = timeout_ms;
    watchdog->last_kick = furi_get_tick();
    watchdog->active = true;
    
    // Start timer - check every 500ms
    furi_timer_start(watchdog->timer, 500);
    
    return true;
}

void predator_watchdog_kick(PredatorApp* app) {
    UNUSED(app);
    
    if(!watchdog) return;
    
    // Update last kick time
    watchdog->last_kick = furi_get_tick();
}

void predator_watchdog_stop(PredatorApp* app) {
    UNUSED(app);
    
    if(!watchdog || !watchdog->timer) return;
    
    // Stop timer
    furi_timer_stop(watchdog->timer);
    watchdog->active = false;
}

void predator_watchdog_tick(PredatorApp* app) {
    // This is called from the main event loop
    if(!app || !watchdog || !watchdog->active) return;
    
    // Kick the watchdog from the main thread
    predator_watchdog_kick(app);
}
