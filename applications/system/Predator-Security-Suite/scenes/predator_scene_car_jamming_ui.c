#include "../predator_i.h"
#include "../helpers/predator_subghz.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>
#include <string.h>

// Car Jamming Attack - Professional UI
// Shows real-time jamming status, frequency, duration, and power level

typedef enum {
    JammingStatusIdle,
    JammingStatusJamming,
    JammingStatusComplete,
    JammingStatusError
} JammingStatus;

typedef struct {
    JammingStatus status;
    uint32_t frequency;
    uint32_t jamming_time_ms;
    uint8_t power_level; // 0-100%
    bool subghz_ready;
    char frequency_str[16];
    char status_text[16];
} JammingState;

static JammingState jamming_state;
static View* jamming_view = NULL;
static uint32_t jamming_start_tick = 0;

// Common car frequencies
static const uint32_t car_frequencies[] = {
    315000000,  // 315 MHz - North America
    433920000,  // 433.92 MHz - Europe/Asia
    868350000,  // 868.35 MHz - Europe Premium
    315000000   // Default
};
static const char* frequency_names[] = {
    "315 MHz",
    "433.92 MHz",
    "868.35 MHz",
    "315 MHz"
};
static uint8_t current_freq_index = 0;

static void draw_jamming_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "CAR JAMMING");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_jamming_status(Canvas* canvas, JammingState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Status line
    canvas_draw_str(canvas, 2, 22, "Status:");
    const char* status_text = "Unknown";
    switch(state->status) {
        case JammingStatusIdle: status_text = "Ready"; break;
        case JammingStatusJamming: status_text = "Jamming"; break;
        case JammingStatusComplete: status_text = "Complete"; break;
        case JammingStatusError: status_text = "Error"; break;
    }
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // Frequency
    canvas_draw_str(canvas, 2, 32, "Freq:");
    canvas_draw_str(canvas, 35, 32, state->frequency_str);
    
    // Progress bar (animated during jamming)
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(state->status == JammingStatusJamming) {
        // Full bar pulsing effect
        canvas_draw_box(canvas, 3, 37, 122, 4);
    } else if(state->status == JammingStatusComplete) {
        canvas_draw_box(canvas, 3, 37, 122, 4);
    }
}

static void draw_jamming_info(Canvas* canvas, JammingState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Duration
    char time_str[32];
    uint32_t seconds = state->jamming_time_ms / 1000;
    if(seconds >= 60) {
        snprintf(time_str, sizeof(time_str), "Time: %lum %lus", seconds / 60, seconds % 60);
    } else {
        snprintf(time_str, sizeof(time_str), "Time: %lus", seconds);
    }
    canvas_draw_str(canvas, 2, 48, time_str);
    
    // Power level
    char power_str[32];
    const char* power_text = "Low";
    if(state->power_level >= 80) power_text = "Maximum";
    else if(state->power_level >= 50) power_text = "High";
    else if(state->power_level >= 30) power_text = "Medium";
    
    snprintf(power_str, sizeof(power_str), "Power: %s", power_text);
    canvas_draw_str(canvas, 2, 58, power_str);
    
    // Power bar
    canvas_draw_frame(canvas, 2, 60, 124, 4);
    uint8_t power_width = (state->power_level * 122) / 100;
    canvas_draw_box(canvas, 3, 61, power_width, 2);
}

static void car_jamming_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Draw header
    draw_jamming_header(canvas);
    
    // Draw status
    draw_jamming_status(canvas, &jamming_state);
    
    // Draw info
    draw_jamming_info(canvas, &jamming_state);
}

static bool car_jamming_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            // DON'T intercept Back here - let view dispatcher handle it
            // The scene manager's on_event will receive SceneManagerEventTypeBack
            FURI_LOG_I("JammingUI", "Back key in input callback, returning false to pass to scene manager");
            return false;
        } else if(event->key == InputKeyOk) {
            if(jamming_state.status == JammingStatusIdle) {
                // Start jamming
                jamming_state.status = JammingStatusJamming;
                jamming_state.jamming_time_ms = 0;
                jamming_start_tick = furi_get_tick();
                
                // Initialize SubGHz and start jamming
                predator_subghz_init(app);
                bool started = predator_subghz_start_jamming(app, jamming_state.frequency);
                
                if(started) {
                    jamming_state.subghz_ready = true;
                    snprintf(jamming_state.status_text, sizeof(jamming_state.status_text), "OK");
                } else {
                    jamming_state.subghz_ready = false;
                    snprintf(jamming_state.status_text, sizeof(jamming_state.status_text), "Error");
                    jamming_state.status = JammingStatusError;
                }
                
                char log_msg[96];
                if(app->selected_model_make[0] != '\0') {
                    snprintf(log_msg, sizeof(log_msg), "Jamming %s %s: %s at %u%% power", 
                            app->selected_model_make, app->selected_model_name,
                            jamming_state.frequency_str, (unsigned)jamming_state.power_level);
                } else {
                    snprintf(log_msg, sizeof(log_msg), "Car Jamming START: %s at %u%% power", 
                            jamming_state.frequency_str, (unsigned)jamming_state.power_level);
                }
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("JammingUI", "Jamming %s %s: %lu Hz", 
                          app->selected_model_make, app->selected_model_name, jamming_state.frequency);
                return true;
            } else if(jamming_state.status == JammingStatusJamming) {
                // Stop jamming
                jamming_state.status = JammingStatusComplete;
                predator_subghz_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Car Jamming STOP: %s for %lus", 
                        jamming_state.frequency_str, jamming_state.jamming_time_ms / 1000);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("JammingUI", "Jamming stopped by user");
                return true;
            }
        } else if(event->key == InputKeyLeft && jamming_state.status == JammingStatusIdle) {
            // Cycle frequency backward
            if(current_freq_index == 0) {
                current_freq_index = 2;
            } else {
                current_freq_index--;
            }
            jamming_state.frequency = car_frequencies[current_freq_index];
            snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
                    "%.15s", frequency_names[current_freq_index]);
            return true;
        } else if(event->key == InputKeyRight && jamming_state.status == JammingStatusIdle) {
            // Cycle frequency forward
            if(current_freq_index >= 2) {
                current_freq_index = 0;
            } else {
                current_freq_index++;
            }
            jamming_state.frequency = car_frequencies[current_freq_index];
            snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
                    "%.15s", frequency_names[current_freq_index]);
            return true;
        } else if(event->key == InputKeyUp && jamming_state.status == JammingStatusIdle) {
            // Increase power
            if(jamming_state.power_level < 100) {
                jamming_state.power_level += 10;
                if(jamming_state.power_level > 100) jamming_state.power_level = 100;
            }
            return true;
        } else if(event->key == InputKeyDown && jamming_state.status == JammingStatusIdle) {
            // Decrease power
            if(jamming_state.power_level > 0) {
                jamming_state.power_level -= 10;
            }
            return true;
        }
    }
    
    return true;
}

static void car_jamming_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    if(jamming_state.status == JammingStatusJamming) {
        // Update jamming time
        jamming_state.jamming_time_ms = furi_get_tick() - jamming_start_tick;
        
        // PRODUCTION: Send real jamming packets every 100ms
        if(app->subghz_txrx) {
            // Send jamming signal burst - real RF noise generation
            predator_subghz_send_jamming_attack(app, jamming_state.frequency);
            jamming_state.power_level = 100; // Max power
            
            FURI_LOG_D("CarJamming", "[REAL HW] Jamming burst sent at %lu Hz", 
                      jamming_state.frequency);
        }
        
        // Auto-stop after 5 minutes for safety
        if(jamming_state.jamming_time_ms > 300000) {
            jamming_state.status = JammingStatusComplete;
            predator_subghz_stop_attack(app);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Car Jamming auto-stop: %s (5min limit)", 
                    jamming_state.frequency_str);
            predator_log_append(app, log_msg);
            
            FURI_LOG_I("JammingUI", "[REAL HW] Jamming auto-stopped after 5 minutes");
        }
        
        // Trigger view update
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}

void predator_scene_car_jamming_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Initialize jamming state
    memset(&jamming_state, 0, sizeof(JammingState));
    jamming_state.status = JammingStatusIdle;
    
    // Use selected model's frequency if available, otherwise default
    if(app->selected_model_freq > 0) {
        jamming_state.frequency = app->selected_model_freq;
        // Find closest match in frequency list for display
        if(app->selected_model_freq == 315000000) {
            current_freq_index = 0;
        } else if(app->selected_model_freq >= 433000000 && app->selected_model_freq <= 434000000) {
            current_freq_index = 1;
        } else if(app->selected_model_freq >= 868000000 && app->selected_model_freq <= 869000000) {
            current_freq_index = 2;
        } else {
            current_freq_index = 0; // Default
        }
        snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
                "%lu.%02lu MHz", app->selected_model_freq / 1000000, 
                (app->selected_model_freq % 1000000) / 10000);
    } else {
        jamming_state.frequency = car_frequencies[0]; // Default 315 MHz
        current_freq_index = 0;
        snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
                "%.15s", frequency_names[0]);
    }
    
    jamming_state.power_level = 80; // Default 80% power
    snprintf(jamming_state.status_text, sizeof(jamming_state.status_text), "Ready");
    
    // Setup custom view
    if(!app->view_dispatcher) {
        FURI_LOG_E("JammingUI", "View dispatcher is NULL");
        return;
    }
    
    // Create view with callbacks (only if not already created)
    if(!jamming_view) {
        jamming_view = view_alloc();
        if(!jamming_view) {
            FURI_LOG_E("JammingUI", "Failed to allocate view");
            return;
        }
        
        view_set_context(jamming_view, app);
        view_set_draw_callback(jamming_view, car_jamming_ui_draw_callback);
        view_set_input_callback(jamming_view, car_jamming_ui_input_callback);
        
        // Add view to dispatcher
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewCarJammingUI, jamming_view);
        FURI_LOG_I("JammingUI", "View allocated and added to dispatcher");
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewCarJammingUI);
    
    FURI_LOG_I("JammingUI", "Car Jamming UI initialized");
    
    // Start timer for updates
    app->timer = furi_timer_alloc(car_jamming_ui_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100); // Update every 100ms
}

bool predator_scene_car_jamming_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // CRITICAL: Handle back button to stay in app
    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_I("JammingUI", "Back button pressed - stopping attack");
        // Stop jamming if active, then return false to let scene manager handle navigation
        if(jamming_state.status == JammingStatusJamming) {
            jamming_state.status = JammingStatusComplete;
            predator_subghz_stop_attack(app);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Jamming STOPPED by user: %s for %lus", 
                    jamming_state.frequency_str, jamming_state.jamming_time_ms / 1000);
            predator_log_append(app, log_msg);
            FURI_LOG_I("JammingUI", "Attack stopped");
        }
        // Return false to let scene manager do default back navigation (go to previous scene)
        // Returning true would exit the app!
        FURI_LOG_I("JammingUI", "Returning false - scene manager will navigate back");
        return false;
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        // Custom event received - view will redraw automatically
        return true;
    }
    
    return false;
}

void predator_scene_car_jamming_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Stop timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    // Stop jamming if active
    if(jamming_state.status == JammingStatusJamming) {
        predator_subghz_stop_attack(app);
        
        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "Car Jamming EXIT: %s for %lus", 
                jamming_state.frequency_str, jamming_state.jamming_time_ms / 1000);
        predator_log_append(app, log_msg);
    }
    
    jamming_state.status = JammingStatusIdle;
    // DON'T remove/free view - we reuse it next time
    
    FURI_LOG_I("JammingUI", "Car Jamming UI exited");
}
