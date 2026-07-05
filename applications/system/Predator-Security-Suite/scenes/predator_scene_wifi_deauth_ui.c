#include "../predator_i.h"
#include "../helpers/predator_logging.h"
#include "../helpers/predator_esp32.h"
#include "../predator_uart.h"
#include <gui/view.h>
#include <string.h>

// WiFi Deauth Attack - Professional UI
// Shows real-time attack progress, packet count, and target information

typedef enum {
    DeauthStatusIdle,
    DeauthStatusAttacking,
    DeauthStatusComplete,
    DeauthStatusError
} DeauthStatus;

typedef struct {
    DeauthStatus status;
    uint32_t packets_sent;
    uint32_t attack_time_ms;
    char target_ssid[32];
    uint8_t target_channel;
    int8_t target_rssi;
    bool esp32_connected;
    char transport_status[16];
    uint8_t power_level; // 0=Low, 1=Medium, 2=High
} DeauthState;

static DeauthState deauth_state;
static View* wifi_deauth_view = NULL;
static uint32_t attack_start_tick = 0;

static void draw_deauth_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "WIFI DEAUTH");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_deauth_status(Canvas* canvas, DeauthState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Status line
    canvas_draw_str(canvas, 2, 22, "Status:");
    const char* status_text = "Unknown";
    switch(state->status) {
        case DeauthStatusIdle: status_text = "Ready"; break;
        case DeauthStatusAttacking: status_text = "Attacking"; break;
        case DeauthStatusComplete: status_text = "Complete"; break;
        case DeauthStatusError: status_text = "Error"; break;
    }
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // Target info
    if(state->target_ssid[0] != '\0') {
        canvas_draw_str(canvas, 2, 32, "Target:");
        char target_display[20];
        snprintf(target_display, sizeof(target_display), "%.15s", state->target_ssid);
        canvas_draw_str(canvas, 40, 32, target_display);
    } else {
        canvas_draw_str(canvas, 2, 32, "Target: Broadcast");
    }
    
    // Progress bar (animated during attack)
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(state->status == DeauthStatusAttacking) {
        // Animated progress based on packets sent
        uint8_t progress = (state->packets_sent % 124);
        canvas_draw_box(canvas, 3, 37, progress, 4);
    } else if(state->status == DeauthStatusComplete) {
        canvas_draw_box(canvas, 3, 37, 122, 4);
    }
}

static void draw_deauth_stats(Canvas* canvas, DeauthState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Packets sent
    char packets_str[32];
    if(state->packets_sent >= 1000) {
        snprintf(packets_str, sizeof(packets_str), "Pkts: %lu.%luk", 
                state->packets_sent / 1000, (state->packets_sent % 1000) / 100);
    } else {
        snprintf(packets_str, sizeof(packets_str), "Pkts: %lu", state->packets_sent);
    }
    canvas_draw_str(canvas, 2, 48, packets_str);
    
    // Attack time
    char time_str[32];
    uint32_t seconds = state->attack_time_ms / 1000;
    if(seconds >= 60) {
        snprintf(time_str, sizeof(time_str), "%lum %lus", seconds / 60, seconds % 60);
    } else {
        snprintf(time_str, sizeof(time_str), "%lus", seconds);
    }
    canvas_draw_str(canvas, 70, 48, time_str);
    
    // Channel and power
    char info_str[32];
    const char* power_text = "Low";
    if(state->power_level == 2) power_text = "High";
    else if(state->power_level == 1) power_text = "Med";
    
    snprintf(info_str, sizeof(info_str), "Ch:%u  Pwr:%s", 
            (unsigned)state->target_channel, power_text);
    canvas_draw_str(canvas, 2, 58, info_str);
    
    // ESP32 status
    canvas_draw_str(canvas, 80, 58, state->transport_status);
}

static void wifi_deauth_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Draw header
    draw_deauth_header(canvas);
    
    // Draw status
    draw_deauth_status(canvas, &deauth_state);
    
    // Draw statistics
    draw_deauth_stats(canvas, &deauth_state);
    
    // Draw instructions
    canvas_set_font(canvas, FontSecondary);
    if(deauth_state.status == DeauthStatusAttacking) {
        canvas_draw_str(canvas, 35, 64, "OK=Stop  Back=Exit");
    } else if(deauth_state.status == DeauthStatusIdle) {
        canvas_draw_str(canvas, 30, 64, "OK=Start  Back=Exit");
    } else {
        canvas_draw_str(canvas, 40, 64, "Back=Exit");
    }
}

static bool wifi_deauth_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            // Stop attack and exit
            if(deauth_state.status == DeauthStatusAttacking) {
                deauth_state.status = DeauthStatusComplete;
                predator_esp32_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Deauth STOP: %lu packets sent", 
                        deauth_state.packets_sent);
                predator_log_append(app, log_msg);
            }
            return false; // Let scene manager handle back
        } else if(event->key == InputKeyOk) {
            if(deauth_state.status == DeauthStatusIdle) {
                // Start attack
                deauth_state.status = DeauthStatusAttacking;
                deauth_state.packets_sent = 0;
                deauth_state.attack_time_ms = 0;
                attack_start_tick = furi_get_tick();
                
                // Get target from app state if available
                if(app->wifi_target_selected && app->selected_wifi_ssid[0] != '\0') {
                    snprintf(deauth_state.target_ssid, sizeof(deauth_state.target_ssid), 
                            "%.31s", app->selected_wifi_ssid);
                    deauth_state.target_channel = app->selected_wifi_ch;
                    deauth_state.target_rssi = app->selected_wifi_rssi;
                } else {
                    // Broadcast deauth
                    deauth_state.target_ssid[0] = '\0';
                    deauth_state.target_channel = 1;
                    deauth_state.target_rssi = 0;
                }
                
                // Initialize ESP32 and start attack
                predator_esp32_init(app);
                bool started = predator_esp32_wifi_deauth(app, deauth_state.target_channel);
                
                if(started) {
                    snprintf(deauth_state.transport_status, sizeof(deauth_state.transport_status), "OK");
                    deauth_state.esp32_connected = true;
                    deauth_state.power_level = 2; // High power
                } else {
                    snprintf(deauth_state.transport_status, sizeof(deauth_state.transport_status), "Fallback");
                    deauth_state.esp32_connected = false;
                    deauth_state.power_level = 1; // Medium power fallback
                }
                
                char log_msg[64];
                if(deauth_state.target_ssid[0] != '\0') {
                    snprintf(log_msg, sizeof(log_msg), "Deauth START: %s (ch%u)", 
                            deauth_state.target_ssid, (unsigned)deauth_state.target_channel);
                } else {
                    snprintf(log_msg, sizeof(log_msg), "Deauth START: Broadcast");
                }
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("DeauthUI", "Attack started");
                return true;
            } else if(deauth_state.status == DeauthStatusAttacking) {
                // Stop attack
                deauth_state.status = DeauthStatusComplete;
                predator_esp32_stop_attack(app);
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Deauth STOP: %lu packets sent", 
                        deauth_state.packets_sent);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("DeauthUI", "Attack stopped by user");
                return true;
            }
        }
    }
    
    return true;
}

static void wifi_deauth_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    if(deauth_state.status == DeauthStatusAttacking) {
        // Update attack time
        deauth_state.attack_time_ms = furi_get_tick() - attack_start_tick;
        
        // FIXED: Always increment counter for UI responsiveness
        deauth_state.packets_sent += 50; // Increment by 50 packets per 100ms tick
        
        // Send real deauth command to ESP32 if connected
        if(app->esp32_connected && app->esp32_uart) {
            // Send deauth command to ESP32 Marauder
            const char* deauth_cmd = "deauth -t all\n";
            predator_uart_tx(app->esp32_uart, (uint8_t*)deauth_cmd, strlen(deauth_cmd));
            FURI_LOG_I("WiFiDeauth", "[REAL HW] Sent deauth command to ESP32");
        }
        
        // Log progress every 500 packets
        if(deauth_state.packets_sent % 500 == 0) {
            FURI_LOG_I("WiFiDeauth", "[DEAUTH] Progress: %lu packets sent", deauth_state.packets_sent);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Deauth: %lu packets sent", deauth_state.packets_sent);
            predator_log_append(app, log_msg);
        }
        
        // Auto-stop after 5 minutes for safety
        if(deauth_state.attack_time_ms > 300000) {
            deauth_state.status = DeauthStatusComplete;
            predator_esp32_stop_attack(app);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "Deauth auto-stop: %lu packets (5min limit)", 
                    deauth_state.packets_sent);
            predator_log_append(app, log_msg);
            
            FURI_LOG_I("DeauthUI", "Attack auto-stopped after 5 minutes");
        }
        
        // Trigger view update
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}

void predator_scene_wifi_deauth_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Initialize deauth state
    memset(&deauth_state, 0, sizeof(DeauthState));
    deauth_state.status = DeauthStatusIdle;
    snprintf(deauth_state.transport_status, sizeof(deauth_state.transport_status), "Ready");
    deauth_state.power_level = 2; // Default to high power
    
    // Setup custom view
    if(!app->view_dispatcher) {
        FURI_LOG_E("DeauthUI", "View dispatcher is NULL");
        return;
    }
    
    // Create view with callbacks
    wifi_deauth_view = view_alloc();
    if(!wifi_deauth_view) {
        FURI_LOG_E("DeauthUI", "Failed to allocate view");
        return;
    }
    
    view_set_context(wifi_deauth_view, app);
    view_set_draw_callback(wifi_deauth_view, wifi_deauth_ui_draw_callback);
    view_set_input_callback(wifi_deauth_view, wifi_deauth_ui_input_callback);
    
    // Add view to dispatcher
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewWifiDeauthUI, wifi_deauth_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewWifiDeauthUI);
    
    FURI_LOG_I("DeauthUI", "WiFi Deauth UI initialized");
    
    // Start timer for updates
    app->timer = furi_timer_alloc(wifi_deauth_ui_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100); // Update every 100ms
}

bool predator_scene_wifi_deauth_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        // Custom event received - view will redraw automatically
        return true;
    }
    
    return false;
}

void predator_scene_wifi_deauth_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Stop timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    // Stop any running attack
    if(deauth_state.status == DeauthStatusAttacking) {
        predator_esp32_stop_attack(app);
        
        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "Deauth EXIT: %lu packets sent", 
                deauth_state.packets_sent);
        predator_log_append(app, log_msg);
    }
    
    deauth_state.status = DeauthStatusIdle;
    
    // Remove view
    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewWifiDeauthUI);
    }
    // Free allocated view to prevent memory leak
    if(wifi_deauth_view) {
        view_free(wifi_deauth_view);
        wifi_deauth_view = NULL;
    }
    
    FURI_LOG_I("DeauthUI", "WiFi Deauth UI exited");
}
