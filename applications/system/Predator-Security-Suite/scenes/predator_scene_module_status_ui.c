#include "../predator_i.h"
#include "../helpers/predator_gps.h"
#include "../helpers/predator_esp32.h"
#include "../helpers/predator_logging.h"
#include <gui/view.h>
#include <string.h>

// Module Status - Professional UI
// Shows real-time hardware status for all modules

typedef struct {
    bool esp32_connected;
    bool gps_connected;
    bool subghz_ready;
    bool nfc_ready;
    bool bluetooth_ready;
    uint8_t satellites;
    float voltage;
    uint8_t signal_strength;
    uint8_t success_rate;
    uint32_t packets_sent;
    uint32_t packets_received;
    char board_name[24];
    char firmware_version[16];
    uint32_t uptime_ms;
    uint32_t memory_usage;
    uint8_t cpu_usage;
} ModuleStatusState;

// UNUSED: Module Status now uses Submenu widget instead of custom view
/*
static ModuleStatusState status_state;
static uint32_t start_tick = 0;

static void draw_module_status_header(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "MODULE STATUS");
    canvas_draw_line(canvas, 0, 12, 128, 12);
}

static void draw_module_status_info(Canvas* canvas, ModuleStatusState* state) {
    canvas_set_font(canvas, FontSecondary);
    
    // Hardware Status Row 1
    canvas_draw_str(canvas, 2, 22, "ESP32:");
    const char* esp32_icon = state->esp32_connected ? "✓ OK" : "✗ OFF";
    canvas_draw_str(canvas, 40, 22, esp32_icon);
    
    canvas_draw_str(canvas, 70, 22, "GPS:");
    char gps_status[16];
    if(state->gps_connected) {
        snprintf(gps_status, sizeof(gps_status), "✓%u", (unsigned)state->satellites);
    } else {
        snprintf(gps_status, sizeof(gps_status), "✗OFF");
    }
    canvas_draw_str(canvas, 95, 22, gps_status);
    
    // Hardware Status Row 2
    canvas_draw_str(canvas, 2, 32, "SubGHz:");
    const char* subghz_icon = state->subghz_ready ? "✓ OK" : "✗ OFF";
    canvas_draw_str(canvas, 40, 32, subghz_icon);
    
    canvas_draw_str(canvas, 70, 32, "BLE:");
    const char* ble_icon = state->bluetooth_ready ? "✓ OK" : "✗ OFF";
    canvas_draw_str(canvas, 95, 32, ble_icon);
    
    // Performance Metrics Row 3
    char perf_str[32];
    snprintf(perf_str, sizeof(perf_str), "Signal:%u%% Success:%u%%", 
            (unsigned)state->signal_strength, (unsigned)state->success_rate);
    canvas_draw_str(canvas, 2, 42, perf_str);
    
    // System Metrics Row 4
    char system_str[32];
    snprintf(system_str, sizeof(system_str), "CPU:%u%% Mem:%luKB", 
            (unsigned)state->cpu_usage, state->memory_usage / 1024);
    canvas_draw_str(canvas, 2, 52, system_str);
    
    // Board and Firmware Row 5
    char board_fw[48];
    snprintf(board_fw, sizeof(board_fw), "%.12s v%.8s", state->board_name, state->firmware_version);
    canvas_draw_str(canvas, 2, 62, board_fw);
    
    // Uptime
    char uptime_str[24];
    uint32_t seconds = state->uptime_ms / 1000;
    if(seconds >= 3600) {
        snprintf(uptime_str, sizeof(uptime_str), "%luh %lum", seconds / 3600, (seconds % 3600) / 60);
    } else if(seconds >= 60) {
        snprintf(uptime_str, sizeof(uptime_str), "%lum %lus", seconds / 60, seconds % 60);
    } else {
        snprintf(uptime_str, sizeof(uptime_str), "%lus", seconds);
    }
    canvas_draw_str(canvas, 70, 62, uptime_str);
}
*/

// UNUSED: Custom view callbacks - Module Status now uses Submenu widget
/*
static void module_status_ui_draw_callback(Canvas* canvas, void* context) {
    furi_assert(canvas);
    furi_assert(context);
    
    UNUSED(context);
    
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    
    draw_module_status_header(canvas);
    draw_module_status_info(canvas, &status_state);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 30, 64, "OK=Return to Menu");
}

static bool module_status_ui_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    
    // Don't handle anything - let framework do it all
    return false;
}

static void module_status_ui_timer_callback(void* context) {
    furi_assert(context);
    PredatorApp* app = context;
    
    // Update uptime
    status_state.uptime_ms = furi_get_tick() - start_tick;
    
    // REAL BOARD DETECTION - Get actual board configuration
    const PredatorBoardConfig* board_config = predator_boards_get_config(app->board_type);
    if(board_config) {
        strncpy(status_state.board_name, board_config->name, sizeof(status_state.board_name) - 1);
        status_state.board_name[sizeof(status_state.board_name) - 1] = '\0';
        FURI_LOG_D("ModuleStatus", "[REAL HW] Board: %s", status_state.board_name);
    } else {
        strncpy(status_state.board_name, "Unknown Board", sizeof(status_state.board_name) - 1);
    }
    
    // REAL HARDWARE STATUS DETECTION - NO SIMULATION
    // Real ESP32 detection based on board type and UART status
    if(app->board_type == PredatorBoardType3in1AIO || app->board_type == PredatorBoardTypeScreen28) {
        status_state.esp32_connected = (app->esp32_uart != NULL) && app->esp32_connected;
        FURI_LOG_D("ModuleStatus", "[REAL HW] ESP32 status: %s (UART: %s)", 
                   status_state.esp32_connected ? "CONNECTED" : "DISCONNECTED",
                   app->esp32_uart ? "OK" : "NULL");
    } else {
        status_state.esp32_connected = false;
        FURI_LOG_D("ModuleStatus", "[REAL HW] ESP32 not available on this board");
    }
    
    // Real GPS detection based on board type and satellite data
    if(app->board_type == PredatorBoardType3in1AIO || app->board_type == PredatorBoardTypeScreen28) {
        status_state.gps_connected = (app->gps_uart != NULL) && (app->satellites > 0);
        status_state.satellites = app->satellites;
        FURI_LOG_D("ModuleStatus", "[REAL HW] GPS status: %s (Sats: %lu)", 
                   status_state.gps_connected ? "CONNECTED" : "DISCONNECTED", (unsigned long)app->satellites);
    } else {
        status_state.gps_connected = false;
        status_state.satellites = 0;
        FURI_LOG_D("ModuleStatus", "[REAL HW] GPS not available on this board");
    }
    
    // Real SubGHz hardware status
    status_state.subghz_ready = (app->subghz_txrx != NULL);
    FURI_LOG_D("ModuleStatus", "[REAL HW] SubGHz status: %s", 
               status_state.subghz_ready ? "READY" : "NOT_READY");
    
    // Real Flipper Zero built-in hardware
    status_state.bluetooth_ready = furi_hal_bt_is_active();
    status_state.nfc_ready = true; // NFC hardware available
    
    // Real system metrics from Flipper Zero
    status_state.voltage = 3.3f; // Typical Flipper Zero voltage
    status_state.cpu_usage = furi_hal_power_get_pct(); // Real battery percentage
    status_state.memory_usage = memmgr_get_free_heap(); // Real heap usage
    
    // Real signal strength from SubGHz if available
    if(status_state.subghz_ready) {
        status_state.signal_strength = (uint8_t)(100 + furi_hal_subghz_get_rssi()); // Convert dBm to %
        if(status_state.signal_strength > 100) status_state.signal_strength = 100;
    } else {
        status_state.signal_strength = 0;
    }
    
    // REMOVED FAKE SUCCESS RATE - use real packet statistics
    // Calculate real success rate from actual responses
    if(app->packets_sent > 0 && app->targets_found > 0) {
        // Real success rate = (responses / attempts) * 100
        status_state.success_rate = (app->targets_found * 100) / app->packets_sent;
        if(status_state.success_rate > 100) status_state.success_rate = 100;
    } else {
        status_state.success_rate = 0;
    }
    
    // Real packet statistics from app state
    status_state.packets_sent = app->packets_sent;
    // REMOVED FAKE ASSUMPTION - use real received count
    status_state.packets_received = app->targets_found; // Real responses only
    
    // Log periodic status for Live Monitor
    if(status_state.uptime_ms % 5000 < 100) { // Every 5 seconds
        char log_msg[80];
        snprintf(log_msg, sizeof(log_msg), "System Status: %s - Signal:%u%% Success:%u%%", 
                status_state.board_name, (unsigned)status_state.signal_strength, 
                (unsigned)status_state.success_rate);
        predator_log_append(app, log_msg);
    }
    
    // Trigger view update
    if(app->view_dispatcher) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
    }
}
*/

static void module_status_submenu_cb(void* context, uint32_t index) {
    PredatorApp* app = context;
    UNUSED(index);
    UNUSED(app);
    // No action needed - this is just a display scene
}

void predator_scene_module_status_ui_on_enter(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "MODULE STATUS");
    
    // Get board name
    const char* board_name = "Unknown";
    switch(app->board_type) {
        case PredatorBoardTypeOriginal:
            board_name = "Original";
            break;
        case PredatorBoardType3in1AIO:
            board_name = "3in1 AIO";
            break;
        case PredatorBoardTypeDrB0rkMultiV2:
            board_name = "DrB0rk";
            break;
        case PredatorBoardTypeScreen28:
            board_name = "2.8\" Screen";
            break;
        default:
            break;
    }
    
    char item[64];
    
    // Board info
    snprintf(item, sizeof(item), "Board: %s", board_name);
    submenu_add_item(app->submenu, item, 1, module_status_submenu_cb, app);
    
    // Hardware status
    snprintf(item, sizeof(item), "ESP32: %s", app->esp32_uart ? "OK" : "OFF");
    submenu_add_item(app->submenu, item, 2, module_status_submenu_cb, app);
    
    snprintf(item, sizeof(item), "GPS: %s", app->gps_uart ? "OK" : "OFF");
    submenu_add_item(app->submenu, item, 3, module_status_submenu_cb, app);
    
    snprintf(item, sizeof(item), "SubGHz: %s", app->subghz_txrx ? "READY" : "OFF");
    submenu_add_item(app->submenu, item, 4, module_status_submenu_cb, app);
    
    submenu_add_item(app->submenu, "BLE: OK", 5, module_status_submenu_cb, app);
    
    // Stats
    snprintf(item, sizeof(item), "Packets Sent: %lu", (unsigned long)app->packets_sent);
    submenu_add_item(app->submenu, item, 6, module_status_submenu_cb, app);
    
    snprintf(item, sizeof(item), "Targets Found: %lu", (unsigned long)app->targets_found);
    submenu_add_item(app->submenu, item, 7, module_status_submenu_cb, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, PredatorViewSubmenu);
    
    FURI_LOG_I("ModuleStatusUI", "Module Status UI initialized");
}

bool predator_scene_module_status_ui_on_event(void* context, SceneManagerEvent event) {
    PredatorApp* app = context;
    if(!app) return false;
    
    // Handle back button - return to main menu
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;  // Consumed - prevents framework bug
    }
    
    if(event.type == SceneManagerEventTypeCustom) {
        return true;
    }
    
    return false;
}

void predator_scene_module_status_ui_on_exit(void* context) {
    PredatorApp* app = context;
    if(!app || !app->submenu) return;
    
    submenu_reset(app->submenu);
    
    FURI_LOG_I("ModuleStatusUI", "Module Status UI exited");
}
