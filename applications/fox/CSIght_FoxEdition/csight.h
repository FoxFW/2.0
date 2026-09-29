#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include "wifi/esp_at.h"

#define MESH_MAX_NODES 4

#define CSIGHT_UART_BAUD     115200
#define CSIGHT_LINE_WAIT_MS  1500

typedef enum {
    DisplayModeRadar     = 0,
    DisplayModeWaterfall = 1,
    DisplayModeProximity = 2,
    DisplayModeVitals    = 3,
    DisplayModeMesh      = 4,
    DisplayModeHeatmap   = 5,
} DisplayMode;
#define DISPLAY_MODE_COUNT 6

typedef enum {
    AppStateBooting,
    AppStateEsp32Check,
    AppStateEsp32NotFound,
    AppStateMainMenu,
    AppStateConnectSettings,
    AppStateConnecting,

    AppStateScanning,
    AppStateWebUI,
    AppStateSettings,
    AppStateAbout,
    AppStateMeshConfig,
} AppState;

#define MAIN_MENU_COUNT 4
typedef enum {
    MenuItemScan     = 0,
    MenuItemWebUI    = 1,
    MenuItemSettings = 2,
    MenuItemAbout    = 3,
} MainMenuItem;

#define SETTINGS_COUNT 13
typedef enum {
    SettingSensitivity  = 0,
    SettingChannel      = 1,
    SettingAlertThresh  = 2,
    SettingRescanCh     = 3,
    SettingMeshConfig   = 4,
    SettingForgetNodes  = 5,
    SettingSdLogging    = 6,
    SettingPathloss     = 7,
    SettingTestAlert    = 8,
    SettingPreset       = 9,
    SettingScheduleStart = 10,
    SettingScheduleEnd   = 11,
    SettingConnection    = 12,
} SettingItem;

#define RADAR_CX    42
#define RADAR_CY    32
#define RADAR_R     28
#define TARGET_FLASH_MS  800

extern const int8_t SIN64[64];
extern const int8_t COS64[64];

#define MAX_BLIPS 8
typedef struct {
    int8_t  x, y;
    uint8_t age;
    uint8_t intensity;
} RadarBlip;

#define WATERFALL_COLS   80
#define WATERFALL_HEIGHT 40
typedef struct {
    uint8_t cols[WATERFALL_COLS][WATERFALL_HEIGHT / 4];
} WaterfallBuf;

#define HEATMAP_GRID 8
typedef struct {

    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;
    NotificationApp*  notifications;

    EspAt*      esp_at;
    FuriThread* uart_thread;

    AppState    state;
    DisplayMode display_mode;
    uint8_t     sensitivity;
    bool        target_acquired;
    uint32_t    target_ts;

    bool     esp32_probe_ok;
    uint32_t esp32_check_start_tick;
    bool     esp32_probe_tried_alt;
    bool     esp32_check_focus_settings;

    char    chip_name[16];
    uint8_t csi_support;
    uint8_t fw_major;
    uint8_t fw_minor;
    bool    web_ui_active;

    uint8_t esp32_uart_channel;
    bool    config_exists;

    RadarBlip blips[MAX_BLIPS];
    uint8_t   sweep_angle;
    uint8_t   motion_intensity;
    uint8_t   proximity;

    WaterfallBuf waterfall;
    uint8_t      wf_write_col;

    uint8_t breathing_bpm;
    uint8_t heart_bpm;
    bool    vitals_valid;

    int16_t mesh_node_x_cm[MESH_MAX_NODES];
    int16_t mesh_node_y_cm[MESH_MAX_NODES];
    bool    mesh_node_active[MESH_MAX_NODES];
    uint32_t mesh_node_last_seen_tick[MESH_MAX_NODES];

    uint8_t heatmap[HEATMAP_GRID][HEATMAP_GRID];
    uint32_t heatmap_last_decay_tick;

    uint8_t schedule_start_hour;
    uint8_t schedule_end_hour;
    uint8_t mesh_node_intensity[MESH_MAX_NODES];
    uint8_t mesh_node_proximity[MESH_MAX_NODES];
    bool    mesh_has_estimate;
    int16_t mesh_est_x_cm;
    int16_t mesh_est_y_cm;
    uint8_t mesh_config_node_idx;
    bool    mesh_config_edit_y;

    bool     node_found_pending;
    uint8_t  node_found_id;
    uint32_t node_found_ts;

    bool log_enabled;

    uint8_t pathloss_gamma_x10;

    uint8_t wifi_channel;

    bool    alert_armed;
    uint8_t alert_threshold;
    bool    alert_triggered;
    uint32_t alert_ts;

    uint32_t session_start_tick;
    uint32_t motion_count;

    uint8_t menu_idx;
    uint8_t settings_idx;
    uint8_t boot_frame;
} CSIghtApp;

CSIghtApp* csight_app_alloc(void);
void       csight_app_free(CSIghtApp* app);
int32_t    csight_app(void* p);

void csight_uart_init(CSIghtApp* app);
void csight_uart_deinit(CSIghtApp* app);
void csight_uart_send(CSIghtApp* app, const uint8_t* data, size_t len);
void csight_send_probe(CSIghtApp* app);
void csight_send_handshake(CSIghtApp* app);
void csight_send_start(CSIghtApp* app);
void csight_send_stop(CSIghtApp* app);
void csight_send_sensitivity(CSIghtApp* app);
void csight_send_mode(CSIghtApp* app);
void csight_send_calibrate(CSIghtApp* app);
void csight_send_channel(CSIghtApp* app);
void csight_send_channel_auto(CSIghtApp* app);
void csight_send_forget_nodes(CSIghtApp* app);
void csight_send_node_positions(CSIghtApp* app);
void csight_send_pathloss_gamma(CSIghtApp* app);

void csight_draw_boot(Canvas* c, CSIghtApp* app);
void csight_draw_esp32_check(Canvas* c, CSIghtApp* app);
void csight_draw_esp32_not_found(Canvas* c, CSIghtApp* app);
void csight_draw_main_menu(Canvas* c, CSIghtApp* app);
void csight_draw_connect_settings(Canvas* c, CSIghtApp* app);
void csight_draw_radar(Canvas* c, CSIghtApp* app);
void csight_draw_waterfall(Canvas* c, CSIghtApp* app);
void csight_draw_proximity(Canvas* c, CSIghtApp* app);
void csight_draw_vitals(Canvas* c, CSIghtApp* app);
void csight_draw_mesh(Canvas* c, CSIghtApp* app);
void csight_draw_heatmap(Canvas* c, CSIghtApp* app);

void csight_heatmap_add(CSIghtApp* app, int16_t x_cm, int16_t y_cm);
void csight_draw_mesh_config(Canvas* c, CSIghtApp* app);
void csight_draw_webui(Canvas* c, CSIghtApp* app);
void csight_draw_settings(Canvas* c, CSIghtApp* app);
void csight_draw_about(Canvas* c, CSIghtApp* app);

bool csight_config_load(CSIghtApp* app);
void csight_config_save(CSIghtApp* app);

void csight_add_blip(CSIghtApp* app, uint8_t intensity, uint8_t proximity);
void csight_tick(CSIghtApp* app);
void csight_send_webui_toggle(CSIghtApp* app);
