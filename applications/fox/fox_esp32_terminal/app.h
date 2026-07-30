#pragma once

#include <furi.h>
#include <furi_hal_serial_types.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>

#include "esp_at.h"
#include "fox_splash.h"
#include "fox_scroll_text.h"

typedef enum {
    FoxTerminalViewSplash,
    FoxTerminalViewMainMenu,
    FoxTerminalViewMessage,
    FoxTerminalViewConnectSettings,
    FoxTerminalViewTerminal,
    FoxTerminalViewTextInput,
    FoxTerminalViewLogList,
    FoxTerminalViewLogContent,
} FoxTerminalView;

typedef enum {
    MainMenuViewTerminal,
    MainMenuSendCommand,
    MainMenuViewLogs,
    MainMenuItemCount,
} MainMenuIndex;

#define FOX_TEXT_INPUT_BUFFER_MAX 192
#define FOX_LOG_FILENAME_MAX      40
#define FOX_LOG_FILE_LIST_MAX     64
#define FOX_LOG_PATH_MAX          96

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    FoxSplash* splash;

    EspAt* esp_at;
    size_t pin_option_index;
    size_t baud_option_index;

    bool esp32_detected;
    FoxTerminalView current_view;

    bool message_view_detecting;
    bool message_view_not_detected_focus_left;
    bool message_view_serial_busy;
    bool message_view_serial_retrying;
    bool message_view_serial_retry_failed;
    uint8_t serial_busy_countdown;
    FuriTimer* serial_busy_timer;
    FuriTimer* serial_retry_timer;
    View* message_view;

    View* connect_settings_view;
    uint8_t connect_settings_selected;

    View* main_menu_view;
    size_t main_menu_selected;
    size_t main_menu_scroll;

    FuriString* log;
    View* terminal_view;
    size_t terminal_scroll;
    bool terminal_paused;

    size_t terminal_paused_skipped_lines;
    FuriTimer* terminal_poll_timer;

    TextInput* text_input;
    char text_input_buffer[FOX_TEXT_INPUT_BUFFER_MAX];
    char last_command[FOX_TEXT_INPUT_BUFFER_MAX];

    bool send_from_terminal;

    Storage* storage;
    File* session_log_file;
    bool session_log_open;
    char session_log_path[FOX_LOG_PATH_MAX];

    View* log_list_view;
    char log_files[FOX_LOG_FILE_LIST_MAX][FOX_LOG_FILENAME_MAX];
    size_t log_file_count;
    size_t log_file_selected;
    size_t log_file_scroll;
    FuriTimer* log_list_scroll_timer;
    FoxScrollText log_list_text_anim;

    View* log_content_view;
    FuriString* log_content;
    char log_content_filename[FOX_LOG_FILENAME_MAX];
    size_t log_content_scroll;
} App;

void app_log(App* app, const char* fmt, ...);
void app_switch_to_main_menu(App* app);
void app_show_terminal(App* app);
void app_show_send_command(App* app, bool from_terminal);
void app_show_log_list(App* app);
void app_show_log_content(App* app, const char* filename);

bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms);

size_t app_pin_option_count(void);
const char* app_pin_option_label(size_t index);
size_t app_baud_option_count(void);
uint32_t app_baud_option_value(size_t index);

bool app_probe_uart_selected(App* app);
void app_retry_detection(App* app);
