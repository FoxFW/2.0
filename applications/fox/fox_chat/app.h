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
    FoxCommanderViewTextInput,
    FoxCommanderViewConnectSettings,
    FoxCommanderViewChatList,
    FoxCommanderViewChatDetail,
} FoxCommanderView;

typedef enum {
    MenuContextChat,
} MenuContext;

typedef enum {
    TextInputPurposeNone,
    TextInputPurposeChatMessage,
} TextInputPurpose;

#define FOX_TEXT_INPUT_BUFFER_MAX 192

#define FOX_CHAT_MESSAGE_MAX 10
#define FOX_CHAT_MESSAGE_TEXT_MAX 110

typedef struct {
    char time[6];
    char text[FOX_CHAT_MESSAGE_TEXT_MAX + 1];
} ChatMessage;

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
    bool message_view_wifi_not_connected;
    View* message_view;

    MenuContext menu_context;
    MenuContext menu_return_context;

    FuriString* log;
    View* terminal_view;
    size_t terminal_scroll;

    TextInputPurpose text_input_purpose;
    char text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX];
    char saved_message[FOX_TEXT_INPUT_BUFFER_MAX];

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    View* chat_list_view;
    View* chat_detail_view;
    ChatMessage chat_messages[FOX_CHAT_MESSAGE_MAX];
    size_t chat_message_count;
    size_t chat_message_selected;
    size_t chat_message_scroll;
    size_t chat_detail_scroll;

    bool launch_commander;
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
void app_launch_commander(App* app);
void app_show_text_input_restore(App* app, const char* header, TextInputPurpose purpose);

