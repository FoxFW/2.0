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
#include "qrcodegen.h"

#define FOX_QR_MAX_VERSION 10
#define FOX_QR_BUFFER_LEN qrcodegen_BUFFER_LEN_FOR_VERSION(FOX_QR_MAX_VERSION)

#define FOX_WIFI_SSID_MAX 33

#define FOX_PORTAL_MAX_FIELDS 12
#define FOX_PORTAL_FIELD_KEY_MAX 16
#define FOX_PORTAL_HTML_TRANSFER_MAX 1024

typedef enum {
    FoxCommanderViewSplash,
    FoxCommanderViewMenu,
    FoxCommanderViewMessage,
    FoxCommanderViewTerminal,
    FoxCommanderViewTextInput,
    FoxCommanderViewConnectSettings,
    FoxCommanderViewFoxPortalQr,
} FoxCommanderView;

typedef enum {
    MenuContextFoxPortal,
    MenuContextFoxPortalFields,
    MenuContextFoxPortalFieldDelete,
} MenuContext;

typedef enum {
    TextInputPurposeNone,
    TextInputPurposeFoxPortalSsid,
    TextInputPurposeFoxPortalNewFieldName,
} TextInputPurpose;

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
    FoxCommanderView current_view;

    bool message_view_detecting;
    View* message_view;

    MenuContext menu_context;
    MenuContext menu_return_context;

    FuriString* log;
    View* terminal_view;
    size_t terminal_scroll;

    TextInputPurpose text_input_purpose;
    char text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX];

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    FuriString* portal_ssid;
    View* foxportal_qr_view;
    uint8_t qr_buf[FOX_QR_BUFFER_LEN];
    int qr_size;
    char portal_fields[FOX_PORTAL_MAX_FIELDS][FOX_PORTAL_FIELD_KEY_MAX + 1];
    size_t portal_field_count;
    size_t portal_field_pending_delete;
} App;

void app_log(App* app, const char* fmt, ...);
void app_render_log(App* app);
bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms);
void app_switch_to_menu(App* app, MenuContext ctx);
void app_show_text_input(App* app, const char* header, TextInputPurpose purpose);

void app_menu_item_callback(void* context, uint32_t index);

size_t app_pin_option_count(void);
const char* app_pin_option_label(size_t index);
size_t app_baud_option_count(void);
uint32_t app_baud_option_value(size_t index);

bool app_probe_uart_selected(App* app);
void app_retry_detection(App* app);

