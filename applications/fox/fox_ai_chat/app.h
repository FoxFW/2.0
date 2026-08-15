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
    AiChatViewSplash,
    AiChatViewMenu,
    AiChatViewMessage,
    AiChatViewTerminal,
    AiChatViewTextInput,
    AiChatViewProgress,
    AiChatViewMessageLimit,
} AiChatView;

typedef enum {
    TextInputPurposeNone,
    TextInputPurposeChatMessage,
} TextInputPurpose;

#define FOX_TEXT_INPUT_BUFFER_MAX 192

typedef enum {
    ProgressStageSending,
    ProgressStageReceiving,
} ProgressStage;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    FoxSplash* splash;

    Submenu* submenu;
    TextInput* text_input;

    EspAt* esp_at;
    size_t pin_option_index;

    bool esp32_detected;
    AiChatView current_view;

    bool message_view_detecting;
    bool message_view_wifi_not_connected;
    bool message_view_serial_busy;
    bool message_view_serial_retrying;
    bool message_view_serial_retry_failed;
    uint8_t serial_busy_countdown;
    FuriTimer* serial_busy_timer;
    FuriTimer* serial_retry_timer;
    View* message_view;

    FuriString* log;
    View* terminal_view;
    size_t terminal_scroll;

    TextInputPurpose text_input_purpose;
    char text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX];
    char saved_message[FOX_TEXT_INPUT_BUFFER_MAX];

    View* progress_view;
    ProgressStage progress_stage;

    View* message_limit_view;
    FuriTimer* message_limit_timer;
    uint32_t message_limit_elapsed_offset_sec;
    uint32_t message_limit_start_tick;
    uint32_t message_limit_remaining_sec;

    bool launch_commander;
} App;

void app_log(App* app, const char* fmt, ...);
void app_render_log(App* app);
bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms);
void app_switch_to_menu(App* app);
void app_show_text_input(App* app, const char* header, TextInputPurpose purpose);
void app_show_text_input_restore(App* app, const char* header, TextInputPurpose purpose);

void app_menu_item_callback(void* context, uint32_t index);

void app_retry_detection(App* app);
void app_launch_commander(App* app);
