#include "../predator_i.h"
#include "../helpers/predator_esp32.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>
#include <string.h>

// WiFi Scanner - Professional UI
// Shows real-time scan progress, AP count, signal strength, and status

typedef enum {
    WiFiScanStatusIdle,
    WiFiScanStatusScanning,
    WiFiScanStatusComplete,
    WiFiScanStatusError
} WiFiScanStatus;

typedef struct {
    WiFiScanStatus status;
    uint32_t aps_found;
    uint32_t scan_time_ms;
    int8_t strongest_rssi;
    char strongest_ssid[32];
    bool esp32_connected;
    char transport_status[16];
} WiFiScanState;

static WiFiScanState scan_state;
static uint32_t scan_start_tick = 0;
static View* wifi_scan_view = NULL; // Store view reference for cleanup

static void draw_scan_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "WIFI SCANNER");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_scan_status(Canvas* canvas, WiFiScanState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Status line
    canvas_draw_str(canvas, 2, 22, "Status:");
    const char* status_text = "Unknown";
    switch(state->status) {
        case WiFiScanStatusIdle: status_text = "Ready"; break;
        case WiFiScanStatusScanning: status_text = "Scanning..."; break;
        case WiFiScanStatusComplete: status_text = "Complete"; break;
        case WiFiScanStatusError: status_text = "Error"; break;
    }
    canvas_draw_str(canvas, 45, 22, status_text);
    
    // Transport status
    canvas_draw_str(canvas, 2, 32, "ESP32:");
    canvas_draw_str(canvas, 40, 32, state->transport_status);
    
    // Progress bar (visual feedback)
    canvas_draw_frame(canvas, 2, 36, 124, 6);
    if(state->status == WiFiScanStatusScanning) {
        // Animated progress bar
        uint8_t progress = ((state->scan_time_ms / 100) % 124);
        canvas_draw_box(canvas, 3, 37, progress, 4);
    } else if(state->status == WiFiScanStatusComplete) {
        // Full bar when complete
        canvas_draw_box(canvas, 3, 37, 122, 4);
    }
}

static void draw_scan_results(Canvas* canvas, WiFiScanState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // APs found
    char aps_str[32];
    snprintf(aps_str, sizeof(aps_str), "APs Found: %lu", state->aps_found);
    canvas_draw_str(canvas, 2, 48, aps_str);
    
    // Scan time
    char time_str[32];
    uint32_t seconds = state->scan_time_ms / 1000;
    snprintf(time_str, sizeof(time_str), "Time: %lus", seconds);
    canvas_draw_str(canvas, 70, 48, time_str);
    
    // Strongest signal
    if(state->aps_found > 0 && state->strongest_ssid[0] != '\0') {
        canvas_draw_str(canvas, 2, 58, "Strongest:");
        
        // Truncate SSID if too long
        char display_ssid[20];
        snprintf(display_ssid, sizeof(display_ssid), "%.15s", state->strongest_ssid);
        canvas_draw_str(canvas, 52, 58, display_ssid);
        
        // Signal strength with icon
        char rssi_str[16];
        snprintf(rssi_str, sizeof(rssi_str), "%ddBm", (int)state->strongest_rssi);
        canvas_draw_str(canvas, 2, 64, rssi_str);
        
        // Signal strength bars (visual indicator)
        uint8_t bars = 0;
        if(state->strongest_rssi > -50) bars = 4;
        else if(state->strongest_rssi > -60) bars = 3;
        else if(state->strongest_rssi > -70) bars = 2;
        else if(state->strongest_rssi > -80) bars = 1;
        
        for(uint8_t i = 0; i < bars; i++) {
            canvas_draw_box(canvas, 40 + (i * 4), 64 - (i * 2), 3, 2 + (i * 2));
        }
    } else {
        canvas_draw_str(canvas, 2, 58, "No networks found yet");
    }
}

static void wifi_scan_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    // Draw header
    draw_scan_header(canvas);
    
    // Draw status
    draw_scan_status(canvas, &scan_state);
    
    // Draw results
    draw_scan_results(canvas, &scan_state);
    
    // Draw instructions at bottom
    canvas_set_font(canvas, FontSecondary);
    if(scan_state.status == WiFiScanStatusScanning) {
        canvas_draw_str(canvas, 40, 64, "Back=Stop");
    } else if(scan_state.status == WiFiScanStatusComplete) {
        canvas_draw_str(canvas, 30, 64, "OK=View  Back=Exit");
    } else {
        canvas_draw_str(canvas, 30, 64, "OK=Start  Back=Exit");
    }
}

static bool wifi_scan_ui_input_callback(InputEvent* event, void* context) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyBack) {
            // SAFE STOP: Prevent crash during exit
            if(scan_state.status == WiFiScanStatusScanning) {
                scan_state.status = WiFiScanStatusIdle;
                if(app && app->esp32_uart) {
                    predator_esp32_stop_attack(app);
                }
                predator_log_append(app, "WiFiScan STOP");
                furi_delay_ms(100); // Allow cleanup
            }
            return false; // Let scene manager handle back
        } else if(event->key == InputKeyOk) {
            if(scan_state.status == WiFiScanStatusIdle) {
                // Start scan
                scan_state.status = WiFiScanStatusScanning;
                scan_state.aps_found = 0;
                scan_state.scan_time_ms = 0;
                scan_start_tick = furi_get_tick();
                
                // CLEAR previous scan results
                app->wifi_ap_count = 0;
                memset(app->wifi_ssids, 0, sizeof(app->wifi_ssids));
                
                // Initialize ESP32 and start scan
                predator_esp32_init(app);
                FURI_LOG_I("WiFiScan", "Starting WiFi scan - sending 'scanap' command");
                bool started = predator_esp32_wifi_scan(app);
                FURI_LOG_I("WiFiScan", "WiFi scan command sent: %s", started ? "SUCCESS" : "FAILED");
                
                if(started) {
                    snprintf(scan_state.transport_status, sizeof(scan_state.transport_status), "UART OK");
                    scan_state.esp32_connected = true;
                } else {
                    snprintf(scan_state.transport_status, sizeof(scan_state.transport_status), "Fallback");
                    scan_state.esp32_connected = false;
                }
                
                predator_log_append(app, "WiFiScan START");
                FURI_LOG_I("WiFiScanUI", "Scan started");
                
                return true;
            } else if(scan_state.status == WiFiScanStatusComplete) {
                // View results (could navigate to results scene)
                FURI_LOG_I("WiFiScanUI", "View results requested");
                return true;
            }
        }
    }
    
    return true;
}

static void wifi_scan_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    if(scan_state.status == WiFiScanStatusScanning) {
        // Update scan time
        scan_state.scan_time_ms = furi_get_tick() - scan_start_tick;
        
        // Update AP count from app state
        scan_state.aps_found = app->wifi_ap_count;
        
        // DEBUG: Log current AP count every 5 seconds
        static uint32_t last_log_time = 0;
        if(furi_get_tick() - last_log_time > 5000) {
            FURI_LOG_I("WiFiScan", "Scan progress: %lu APs found, ESP32 connected: %s", 
                scan_state.aps_found, scan_state.esp32_connected ? "YES" : "NO");
            last_log_time = furi_get_tick();
        }
        
        // Find strongest signal
        if(app->wifi_ap_count > 0) {
            int8_t strongest = -100;
            size_t strongest_idx = 0;
            
            for(size_t i = 0; i < app->wifi_ap_count && i < PREDATOR_WIFI_MAX_APS; i++) {
                if(app->wifi_rssi[i] > strongest) {
                    strongest = app->wifi_rssi[i];
                    strongest_idx = i;
                }
            }
            
            scan_state.strongest_rssi = strongest;
            if(strongest_idx < PREDATOR_WIFI_MAX_APS) {
                snprintf(scan_state.strongest_ssid, sizeof(scan_state.strongest_ssid), 
                        "%.31s", app->wifi_ssids[strongest_idx]);
            }
        }
        
        // Auto-complete after 30 seconds
        if(scan_state.scan_time_ms > 30000) {
            scan_state.status = WiFiScanStatusComplete;
            predator_esp32_stop_attack(app);
            
            char log_msg[64];
            snprintf(log_msg, sizeof(log_msg), "WiFiScan complete: %lu APs found", scan_state.aps_found);
            predator_log_append(app, log_msg);
            FURI_LOG_I("WiFiScanUI", "Scan completed: %lu APs", scan_state.aps_found);
        }
        
        // Trigger view update
        if(app->view_dispatcher) {
            view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        }
    }
}

void predator_scene_wifi_scan_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // Initialize scan state
    memset(&scan_state, 0, sizeof(WiFiScanState));
    scan_state.status = WiFiScanStatusIdle;
    snprintf(scan_state.transport_status, sizeof(scan_state.transport_status), "Not started");
    
    // Setup custom view
    if(!app->view_dispatcher) {
        FURI_LOG_E("WiFiScanUI", "View dispatcher is NULL");
        return;
    }
    
    // Create view with callbacks
    wifi_scan_view = view_alloc();
    if(!wifi_scan_view) {
        FURI_LOG_E("WiFiScanUI", "Failed to allocate view");
        return;
    }
    
    view_set_context(wifi_scan_view, app);
    view_set_draw_callback(wifi_scan_view, wifi_scan_ui_draw_callback);
    view_set_input_callback(wifi_scan_view, wifi_scan_ui_input_callback);
    
    // Add view to dispatcher
    view_dispatcher_add_view(app->view_dispatcher, PredatorViewWifiScanUI, wifi_scan_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewWifiScanUI);
    
    FURI_LOG_I("WiFiScanUI", "WiFi Scan UI initialized");
    
    // Start timer for updates
    app->timer = furi_timer_alloc(wifi_scan_ui_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 100); // Update every 100ms
}

bool predator_scene_wifi_scan_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    if(event.type == SceneManagerEventTypeCustom) {
        // Custom event received - view will redraw automatically
        return true;
    }
    
    return false;
}

void predator_scene_wifi_scan_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app) return;
    
    // CRITICAL: Stop timer first to prevent callbacks during cleanup
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
    
    // SAFE STOP: Stop any running scan with NULL checks
    if(scan_state.status == WiFiScanStatusScanning) {
        scan_state.status = WiFiScanStatusIdle;
        if(app->esp32_uart) {
            predator_esp32_stop_attack(app);
        }
        predator_log_append(app, "WiFiScan EXIT");
        furi_delay_ms(50); // Allow ESP32 to process stop
    }
    
    // SAFE: Reset scan state
    memset(&scan_state, 0, sizeof(scan_state));
    
    // CRITICAL: Free allocated view to prevent memory leak
    if(app->view_dispatcher && wifi_scan_view) {
        view_dispatcher_remove_view(app->view_dispatcher, PredatorViewWifiScanUI);
        view_free(wifi_scan_view);
        wifi_scan_view = NULL;
        FURI_LOG_I("WiFiScanUI", "View freed - memory leak prevented");
    }
    
    FURI_LOG_I("WiFiScanUI", "WiFi Scan UI safely exited");
}
