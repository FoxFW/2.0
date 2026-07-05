#include "../predator_i.h"
#include "../helpers/predator_logging.h"
#include "../helpers/predator_esp32.h"
#include "../predator_uart.h"
#include <gui/view.h>
#include <string.h>

// WiFi Evil Twin - Professional UI
// Shows real-time rogue AP status with client count and capture statistics

typedef enum {
    EvilTwinStatusIdle,
    EvilTwinStatusBroadcasting,
    EvilTwinStatusComplete,
    EvilTwinStatusError
} EvilTwinStatus;

typedef struct {
    EvilTwinStatus status;
    char target_ssid[32];
    uint8_t channel;
    uint16_t clients_connected;
    uint16_t handshakes_captured;
    uint32_t broadcast_time_ms;
    bool esp32_connected;
    char auth_type[16];
} EvilTwinState;

static EvilTwinState eviltwin_state;
static View* wifi_evil_twin_view = NULL;
static uint32_t broadcast_start_tick = 0;

static void draw_eviltwin_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "EVIL TWIN");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_eviltwin_status(Canvas* canvas, EvilTwinState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Status line
    canvas_draw_str(canvas, 2, 22, "Status:");
    const char* status_text = "Unknown";
    switch(state->status) {
        case EvilTwinStatusIdle: status_text = "Ready"; break;
        case EvilTwinStatusBroadcasting: status_text = "Broadcasting"; break;
        case EvilTwinStatusComplete: status_text = "Complete"; break;
        case EvilTwinStatusError: status_text = "Error"; break;
    }
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // SSID
    if(state->target_ssid[0] != '\0') {
        canvas_draw_str(canvas, 2, 32, "SSID:");
        char ssid_display[20];
        snprintf(ssid_display, sizeof(ssid_display), "%.15s", state->target_ssid);
        canvas_draw_str(canvas, 35, 32, ssid_display);
    }
    
    // Progress bar (pulsing during broadcast)
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(state->status == EvilTwinStatusBroadcasting) {
        uint8_t progress = ((state->broadcast_time_ms / 50) % 124);
        canvas_draw_box(canvas, 3, 37, progress, 4);
    } else if(state->status == EvilTwinStatusComplete) {
        canvas_draw_box(canvas, 3, 37, 122, 4);
    }
}

static void draw_eviltwin_stats(Canvas* canvas, EvilTwinState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Clients and captures
    char stats_str[32];
    snprintf(stats_str, sizeof(stats_str), "Clients:%u  Cap:%u", 
            (unsigned)state->clients_connected, (unsigned)state->handshakes_captured);
    canvas_draw_str(canvas, 2, 48, stats_str);
    
    // Time
    char time_str[32];
    uint32_t seconds = state->broadcast_time_ms / 1000;
    if(seconds >= 60) {
        snprintf(time_str, sizeof(time_str), "%lum %lus", seconds / 60, seconds % 60);
    } else {
        snprintf(time_str, sizeof(time_str), "%lus", seconds);
    }
    canvas_draw_str(canvas, 2, 58, time_str);
    
    // Channel and auth
    char info_str[32];
    snprintf(info_str, sizeof(info_str), "Ch:%u  %s", 
            (unsigned)state->channel, state->auth_type);
    canvas_draw_str(canvas, 60, 58, info_str);
}

static void wifi_evil_twin_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Draw header
    draw_eviltwin_header(canvas);
    
    // Draw status
    draw_eviltwin_status(canvas, &eviltwin_state);
    
    // Draw statistics
    draw_eviltwin_stats(canvas, &eviltwin_state);
    
    // Draw instructions
    canvas_set_font(canvas, FontSecondary);
    if(eviltwin_state.status == EvilTwinStatusBroadcasting) {
        canvas_draw_str(canvas, 30, 64, "OK=Stop  Back=Exit");
    } else if(eviltwin_state.status == EvilTwinStatusIdle) {
        canvas_draw_str(canvas, 25, 64, "OK=Start  Back=Exit");
    } else {
        canvas_draw_str(canvas, 40, 64, "Back=Exit");
    }
}

static bool wifi_evil_twin_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            // Stop evil twin and exit
            if(eviltwin_state.status == EvilTwinStatusBroadcasting) {
                eviltwin_state.status = EvilTwinStatusComplete;
                predator_esp32_stop_attack(app);
                
                char log_msg[80];
                snprintf(log_msg, sizeof(log_msg), "Evil Twin STOP: %u clients, %u captures", 
                        (unsigned)eviltwin_state.clients_connected, 
                        (unsigned)eviltwin_state.handshakes_captured);
                predator_log_append(app, log_msg);
            }
            return false; // Let scene manager handle back
        } else if(event->key == InputKeyOk) {
            if(eviltwin_state.status == EvilTwinStatusIdle) {
                // Start evil twin
                eviltwin_state.status = EvilTwinStatusBroadcasting;
                eviltwin_state.clients_connected = 0;
                eviltwin_state.handshakes_captured = 0;
                eviltwin_state.broadcast_time_ms = 0;
                broadcast_start_tick = furi_get_tick();
                
                // Get target from app state or use default
                if(app->wifi_target_selected && app->selected_wifi_ssid[0] != '\0') {
                    snprintf(eviltwin_state.target_ssid, sizeof(eviltwin_state.target_ssid), 
                            "%.31s", app->selected_wifi_ssid);
                    eviltwin_state.channel = app->selected_wifi_ch;
                } else {
                    snprintf(eviltwin_state.target_ssid, sizeof(eviltwin_state.target_ssid), "FreeWiFi");
                    eviltwin_state.channel = 6;
                }
                
                snprintf(eviltwin_state.auth_type, sizeof(eviltwin_state.auth_type), "WPA2");
                
                // Initialize ESP32 and start evil twin
                predator_esp32_init(app);
                bool started = predator_esp32_wifi_evil_twin(app);
                eviltwin_state.esp32_connected = started;
                
                char log_msg[64];
                snprintf(log_msg, sizeof(log_msg), "Evil Twin START: %s (ch%u)", 
                        eviltwin_state.target_ssid, (unsigned)eviltwin_state.channel);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("EvilTwinUI", "Evil Twin started: %s", eviltwin_state.target_ssid);
                return true;
            } else if(eviltwin_state.status == EvilTwinStatusBroadcasting) {
                // Stop evil twin
                eviltwin_state.status = EvilTwinStatusComplete;
                predator_esp32_stop_attack(app);
                
                char log_msg[80];
                snprintf(log_msg, sizeof(log_msg), "Evil Twin STOP: %u clients, %u captures", 
                        (unsigned)eviltwin_state.clients_connected, 
                        (unsigned)eviltwin_state.handshakes_captured);
                predator_log_append(app, log_msg);
                
                FURI_LOG_I("EvilTwinUI", "Evil Twin stopped by user");
                return true;
            }
        }
    }
    
    return true;
}

static void wifi_evil_twin_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    if(eviltwin_state.status == EvilTwinStatusBroadcasting) {
        // Update broadcast time
        eviltwin_state.broadcast_time_ms = furi_get_tick() - broadcast_start_tick;
        
        // Send real evil twin commands to ESP32
        if(app->esp32_connected && app->esp32_uart) {
            // Check for client connections every 5 seconds
            if(eviltwin_state.broadcast_time_ms % 5000 < 100) {
                const char* status_cmd = "list -c\n"; // List clients command
                predator_uart_tx(app->esp32_uart, (uint8_t*)status_cmd, strlen(status_cmd));
                FURI_LOG_I("WiFiEvilTwin", "[REAL HW] Checking for connected clients");
                
                // REMOVED FAKE INCREMENT - only count real clients from ESP32 response
                // Real client count from ESP32 response parsing
                if(app->wifi_ap_count > 0 && app->targets_found > eviltwin_state.clients_connected) {
                    eviltwin_state.clients_connected = app->targets_found;
                    FURI_LOG_I("WiFiEvilTwin", "[REAL HW] Client connected: %u total", 
                              eviltwin_state.clients_connected);
                }
            }
            
            // Check for handshake captures - REMOVED AUTO INCREMENT
            if(eviltwin_state.clients_connected > 0 && eviltwin_state.broadcast_time_ms % 10000 < 100) {
                const char* handshake_cmd = "handshake\n";
                predator_uart_tx(app->esp32_uart, (uint8_t*)handshake_cmd, strlen(handshake_cmd));
                FURI_LOG_I("WiFiEvilTwin", "[REAL HW] Checking for handshakes");
                
                // REMOVED FAKE INCREMENT - only count if ESP32 actually captured handshake
                // In production, parse ESP32 response to check if handshake was actually captured
                // Don't increment without confirmation
            }
        }
        
        // Update from app state if available
        if(app->targets_found > 0) {
            eviltwin_state.clients_connected = app->targets_found;
        }
        
        // Trigger view update
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}

void predator_scene_wifi_evil_twin_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Initialize evil twin state
    memset(&eviltwin_state, 0, sizeof(EvilTwinState));
    eviltwin_state.status = EvilTwinStatusIdle;
    
    // Setup custom view
    if(!app->view_dispatcher) {
        FURI_LOG_E("EvilTwinUI", "View dispatcher is NULL");
        return;
    }
    
    // Create view with callbacks
    wifi_evil_twin_view = view_alloc();
    if(!wifi_evil_twin_view) {
        FURI_LOG_E("EvilTwinUI", "Failed to allocate view");
        return;
    }
    
    view_set_context(wifi_evil_twin_view, app);
    view_set_draw_callback(wifi_evil_twin_view, wifi_evil_twin_ui_draw_callback);
    view_set_input_callback(wifi_evil_twin_view, wifi_evil_twin_ui_input_callback);
    
    // Add view to dispatcher
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewWifiEvilTwinUI, wifi_evil_twin_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewWifiEvilTwinUI);
    
    FURI_LOG_I("EvilTwinUI", "WiFi Evil Twin UI initialized");
    
    // Start timer for updates
    app->timer = furi_timer_alloc(wifi_evil_twin_ui_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100); // Update every 100ms
}

bool predator_scene_wifi_evil_twin_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        // Custom event received - view will redraw automatically
        return true;
    }
    
    return false;
}

void predator_scene_wifi_evil_twin_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Stop timer
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    // Stop evil twin
    if(eviltwin_state.status == EvilTwinStatusBroadcasting) {
        predator_esp32_stop_attack(app);
        
        char log_msg[80];
        snprintf(log_msg, sizeof(log_msg), "Evil Twin EXIT: %u clients, %u captures", 
                (unsigned)eviltwin_state.clients_connected, 
                (unsigned)eviltwin_state.handshakes_captured);
        predator_log_append(app, log_msg);
    }
    
    eviltwin_state.status = EvilTwinStatusIdle;
    
    // Remove view
    if(app->view_dispatcher) {
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewWifiEvilTwinUI);
    }
    // Free allocated view to prevent memory leak
    if(wifi_evil_twin_view) {
        view_free(wifi_evil_twin_view);
        wifi_evil_twin_view = NULL;
    }
    
    FURI_LOG_I("EvilTwinUI", "WiFi Evil Twin UI exited");
}
