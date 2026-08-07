#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>

#include "esp_at.h"
#include "fox_splash.h"

typedef enum {
    FoxCommanderViewSplash,
    FoxCommanderViewMenu,
    FoxCommanderViewMessage,
    FoxCommanderViewTerminal,
    FoxCommanderViewNetworkList,
    FoxCommanderViewTextInput,
    FoxCommanderViewSettings,
    FoxCommanderViewStationList,
    FoxCommanderViewConnectSettings,
    FoxCommanderViewSavedWifiList,
} FoxCommanderView;

typedef enum {
    MenuContextMain,
    MenuContextWifi,
    MenuContextWifiConnection,
    MenuContextWifiRecon,
    MenuContextWifiAttacks,
    MenuContextWifiHttp,
    MenuContextWifiSavedAction,
    MenuContextBluetooth,
    MenuContextTagDetect,
    MenuContextScripts,
    MenuContextScriptActions,
} MenuContext;

typedef enum {
    MenuMainWifi,
    MenuMainBluetooth,
    MenuMainTagDetect,
    MenuMainScripts,
    MenuMainSettings,
    MenuMainTerminal,
    MenuMainTerminalCommand,
} MenuMainIndex;

typedef enum {
    TextInputPurposeNone,
    TextInputPurposePassword,
    TextInputPurposeScriptName,
    TextInputPurposeScriptSource,
    TextInputPurposeBeaconSsids,
    TextInputPurposeHttpGetUrl,
    TextInputPurposeHttpPostUrl,
    TextInputPurposeHttpPostBody,
    TextInputPurposeTerminalCommand,
    TextInputPurposeSavedWifiEditPassword,
} TextInputPurpose;

#define FOX_WIFI_NETWORK_MAX 24
#define FOX_WIFI_SSID_MAX    33

typedef struct {
    char ssid[FOX_WIFI_SSID_MAX];
    int rssi;
    bool secure;
    bool saved;
    int scan_index;
} FoxWifiNetwork;

#define FOX_SAVED_WIFI_MAX 8
#define FOX_WIFI_PASS_MAX  64

typedef struct {
    char ssid[FOX_WIFI_SSID_MAX];
    char password[FOX_WIFI_PASS_MAX];
} FoxSavedWifi;

#define FOX_STATION_MAX     16
#define FOX_STATION_MAC_MAX 18

typedef struct {
    char mac[FOX_STATION_MAC_MAX];
    int rssi;
} FoxStation;

#define FOX_SCRIPT_MAX      16
#define FOX_SCRIPT_NAME_MAX 40

typedef struct {
    char name[FOX_SCRIPT_NAME_MAX];
    uint32_t bytes;
} FoxScriptEntry;

#define FOX_TEXT_INPUT_BUFFER_MAX 192

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    FoxSplash* splash;

    Submenu* submenu;
    TextInput* text_input;

    EspAt* esp_at;
    size_t pin_option_index;
    size_t baud_option_index;

    bool esp32_detected;
    bool has_ble;
    bool launch_wifi_connection;
    FoxCommanderView current_view;

    bool message_view_detecting;
    bool     message_view_not_detected_focus_left;
    bool     message_view_serial_busy;
    bool     message_view_serial_retrying;
    bool     message_view_serial_retry_failed;
    bool     message_view_portal_running;
    uint8_t  serial_busy_countdown;
    FuriTimer* serial_busy_timer;
    FuriTimer* serial_retry_timer;
    View* message_view;

    MenuContext menu_context;
    MenuContext menu_return_context;

    bool wifi_menu_connected;

    FuriString* log;
    View* terminal_view;
    size_t terminal_scroll;

    View* network_list_view;
    FoxWifiNetwork networks[FOX_WIFI_NETWORK_MAX];
    size_t network_count;
    size_t network_selected;
    size_t network_scroll;
    bool network_list_for_connect;
    FuriString* pending_ssid;

    View* station_list_view;
    FoxStation stations[FOX_STATION_MAX];
    size_t station_count;
    size_t station_selected;
    size_t station_scroll;
    bool has_target_station;
    char target_station_mac[FOX_STATION_MAC_MAX];

    TextInputPurpose text_input_purpose;
    char text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX];
    FuriString* pending_script_name;
    FuriString* pending_http_url;

    FoxScriptEntry scripts[FOX_SCRIPT_MAX];
    size_t script_count;
    size_t script_selected;

    bool attacks_enabled;
    bool expert_mode;
    uint32_t main_menu_selected;
    View* settings_view;
    uint8_t settings_selected;

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    View* saved_wifi_list_view;
    FoxSavedWifi saved_wifi[FOX_SAVED_WIFI_MAX];
    size_t saved_wifi_count;
    size_t saved_wifi_selected;
    size_t saved_wifi_scroll;
} App;

void app_log(App* app, const char* fmt, ...);
void app_log_raw(App* app, const char* text);
void app_render_log(App* app);
bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms);
void app_switch_to_menu(App* app, MenuContext ctx);
void app_show_text_input(App* app, const char* header, TextInputPurpose purpose);
void app_show_text_input_prefill(
    App* app,
    const char* header,
    TextInputPurpose purpose,
    const char* prefill);

void app_menu_item_callback(void* context, uint32_t index);

size_t app_pin_option_count(void);
const char* app_pin_option_label(size_t index);
size_t app_baud_option_count(void);
uint32_t app_baud_option_value(size_t index);

bool app_probe_uart_selected(App* app);

void app_retry_detection(App* app);
