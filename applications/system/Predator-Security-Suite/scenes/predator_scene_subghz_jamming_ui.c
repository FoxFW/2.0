#include "../predator_i.h"
#include "../helpers/predator_subghz.h"
#include "../helpers/predator_logging.h"
#include "../helpers/predator_constants.h"
#include <gui/view.h>

// SubGHz Jamming - Professional UI
// Real-time RF jamming with frequency selection

typedef enum {
    RFJammingStatusIdle,
    RFJammingStatusJamming,
    RFJammingStatusComplete,
    RFJammingStatusError
} RFJammingStatus;

typedef struct {
    RFJammingStatus status;
    uint32_t frequency;
    uint32_t jamming_time_ms;
    uint8_t power_level;
    uint8_t freq_index;
    bool subghz_ready;
    char frequency_str[16];
} RFJammingState;

static RFJammingState jamming_state;
static uint32_t jamming_start_tick = 0;
static View* subghz_jamming_view = NULL;  // Static view - create once, reuse


static void draw_rf_jamming_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "RF JAMMING");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_rf_jamming_status(Canvas* canvas, RFJammingState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Status line
    canvas_draw_str(canvas, 2, 22, "Status:");
    const char* status_text = "Unknown";
    switch(state->status) {
        case RFJammingStatusIdle: status_text = "Ready"; break;
        case RFJammingStatusJamming: status_text = "Jamming"; break;
        case RFJammingStatusComplete: status_text = "Complete"; break;
        case RFJammingStatusError: status_text = "Error"; break;
    }
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // Frequency
    canvas_draw_str(canvas, 2, 32, "Freq:");
    canvas_draw_str(canvas, 35, 32, state->frequency_str);
    
    // Progress bar (full during jamming)
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(state->status == RFJammingStatusJamming) {
        canvas_draw_box(canvas, 3, 37, 122, 4);
    }
}

static void draw_rf_jamming_info(Canvas* canvas, RFJammingState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Time
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

static void rf_jamming_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    draw_rf_jamming_header(canvas);
    draw_rf_jamming_status(canvas, &jamming_state);
    draw_rf_jamming_info(canvas, &jamming_state);
}

static bool rf_jamming_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            if(jamming_state.status == RFJammingStatusJamming) {
                jamming_state.status = RFJammingStatusComplete;
                predator_subghz_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "RF Jamming STOP: %s for %lus", 
                        jamming_state.frequency_str, jamming_state.jamming_time_ms / 1000);
                predator_log_append(app, log_msg);
            }
            return false;
        } else if(event->key == InputKeyOk) {
            if(jamming_state.status == RFJammingStatusIdle) {
                // Check compliance
                PredatorFeature feat = PredatorFeatureSubGhz433Tx;
                if(jamming_state.frequency == 315000000) feat = PredatorFeatureSubGhz315Tx;
                
                bool allowed = predator_compliance_is_feature_allowed(app, feat, app->authorized);
                if(!allowed) {
                    jamming_state.status = RFJammingStatusError;
                    predator_log_append(app, "RF Jamming DENIED: Authorization required");
                    return true;
                }
                
                jamming_state.status = RFJammingStatusJamming;
                jamming_state.jamming_time_ms = 0;
                jamming_start_tick = furi_get_tick();
                
                predator_subghz_init(app);
                bool started = predator_subghz_start_jamming(app, jamming_state.frequency);
                jamming_state.subghz_ready = started;
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "RF Jamming START: %s at %u%% power", 
                        jamming_state.frequency_str, (unsigned)jamming_state.power_level);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("RFJammingUI", "Jamming started: %lu Hz", jamming_state.frequency);
                return true;
            } else if(jamming_state.status == RFJammingStatusJamming) {
                jamming_state.status = RFJammingStatusComplete;
                predator_subghz_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "RF Jamming STOP: %s for %lus", 
                        jamming_state.frequency_str, jamming_state.jamming_time_ms / 1000);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("RFJammingUI", "Jamming stopped by user");
                return true;
            }
        } else if(event->key == InputKeyLeft && jamming_state.status == RFJammingStatusIdle) {
            if(jamming_state.freq_index > 0) {
                jamming_state.freq_index--;
                jamming_state.frequency = PREDATOR_FREQUENCIES[jamming_state.freq_index];
                snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
                        "%.15s", PREDATOR_FREQUENCY_NAMES[jamming_state.freq_index]);
            }
            return true;
        } else if(event->key == InputKeyRight && jamming_state.status == RFJammingStatusIdle) {
            if(jamming_state.freq_index < (PREDATOR_FREQUENCY_COUNT - 1)) {
                jamming_state.freq_index++;
                jamming_state.frequency = PREDATOR_FREQUENCIES[jamming_state.freq_index];
                snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
                        "%.15s", PREDATOR_FREQUENCY_NAMES[jamming_state.freq_index]);
            }
            return true;
        } else if(event->key == InputKeyUp && jamming_state.status == RFJammingStatusIdle) {
            if(jamming_state.power_level < 100) {
                jamming_state.power_level += 10;
                if(jamming_state.power_level > 100) jamming_state.power_level = 100;
            }
            return true;
        } else if(event->key == InputKeyDown && jamming_state.status == RFJammingStatusIdle) {
            if(jamming_state.power_level > 0) {
                jamming_state.power_level -= 10;
            }
            return true;
        }
    }
    
    return true;
}

static void rf_jamming_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    if(jamming_state.status == RFJammingStatusJamming) {
        jamming_state.jamming_time_ms = furi_get_tick() - jamming_start_tick;
        
        // PRODUCTION: Send real jamming packets every 100ms
        if(app->subghz_txrx) {
            // Send jamming signal burst - real RF noise generation
            uint32_t frequency = PREDATOR_FREQUENCIES[jamming_state.freq_index];
            predator_subghz_send_jamming_attack(app, frequency);
            
            FURI_LOG_D("RFJamming", "[REAL HW] Jamming burst sent at %lu Hz", frequency);
        }
        
        // Auto-stop after 5 minutes for safety
        if(jamming_state.jamming_time_ms > 300000) {
            jamming_state.status = RFJammingStatusComplete;
            predator_subghz_stop_attack(app);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "RF Jamming auto-stop: %s (5min limit)", 
                    jamming_state.frequency_str);
            predator_log_append(app, log_msg);
            
            FURI_LOG_I("RFJammingUI", "[REAL HW] Jamming auto-stopped after 5 minutes");
        }
        
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}

void predator_scene_subghz_jamming_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    memset(&jamming_state, 0, sizeof(RFJammingState));
    jamming_state.status = RFJammingStatusIdle;
    
    // Set default frequency based on region
    jamming_state.freq_index = 1; // Default 433.92 MHz
    if(app->region == PredatorRegionUS || app->region == PredatorRegionJP) {
        jamming_state.freq_index = 0; // 315 MHz for US/JP
    }
    
    jamming_state.frequency = PREDATOR_FREQUENCIES[jamming_state.freq_index];
    jamming_state.power_level = 80;
    snprintf(jamming_state.frequency_str, sizeof(jamming_state.frequency_str), 
            "%.15s", PREDATOR_FREQUENCY_NAMES[jamming_state.freq_index]);
    
    if(!app->view_dispatcher) {
        FURI_LOG_E("RFJammingUI", "View dispatcher is NULL");
        return;
    }
    
    // Create view with callbacks (only if not already created)
    if(!subghz_jamming_view) {
        subghz_jamming_view = view_alloc();
        if(!subghz_jamming_view) {
            FURI_LOG_E("RFJammingUI", "Failed to allocate view");
            return;
        }
        
        view_set_context(subghz_jamming_view, app);
        view_set_draw_callback(subghz_jamming_view, rf_jamming_ui_draw_callback);
        view_set_input_callback(subghz_jamming_view, rf_jamming_ui_input_callback);
        
        view_dispatcher_add_view(app->view_dispatcher, PredatorViewSubGhzJammingUI, subghz_jamming_view);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubGhzJammingUI);
    
    FURI_LOG_I("RFJammingUI", "RF Jamming UI initialized");
    
    app->timer = furi_timer_alloc(rf_jamming_ui_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100);
}

bool predator_scene_subghz_jamming_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - stop jamming and return to previous scene
    if(event.type == SceneManagerEventTypeBack) {
        // Stop jamming attack
        predator_subghz_stop_attack(app);
        FURI_LOG_I("RFJammingUI", "Jamming stopped by user");
        
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    return false;
}

void predator_scene_subghz_jamming_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    if(jamming_state.status == RFJammingStatusJamming) {
        predator_subghz_stop_attack(app);
        
        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "RF Jamming EXIT: %s for %lus", 
                jamming_state.frequency_str, jamming_state.jamming_time_ms / 1000);
        predator_log_append(app, log_msg);
    }
    
    jamming_state.status = RFJammingStatusIdle;
    // DON'T remove/free view - we reuse it next time (match CarKeyBruteforce pattern)
    
    FURI_LOG_I("RFJammingUI", "RF Jamming UI exited");
}
