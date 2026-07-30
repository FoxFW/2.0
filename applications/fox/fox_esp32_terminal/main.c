#include "app.h"
#include "fox_esp32_terminal_icons.h"
#include "main_menu.h"
#include "terminal_screen.h"
#include "log_list_view.h"
#include "log_view_screen.h"
#include "message_view.h"
#include "connect_settings.h"
#include "session_log.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static void action_check_esp32(App* app);
static void on_esp32_connected(App* app);
static void text_input_result_callback(void* context);

typedef struct {
    FuriHalSerialId serial_id;
    const char* label;
} PinOption;

static const PinOption pin_options[] = {
    {FuriHalSerialIdUsart, "13/14 (USART)"},
    {FuriHalSerialIdLpuart, "15/16 (LPUART)"},
};
#define PIN_OPTION_COUNT (sizeof(pin_options) / sizeof(pin_options[0]))

static const uint32_t baud_options[] = {115200};
#define BAUD_OPTION_DEFAULT_INDEX 0

#define FOX_TERMINAL_LOG_MAX_CHARS 4000

#define FOX_TERMINAL_EVENT_SPLASH_DONE      0
#define FOX_TERMINAL_EVENT_SERIAL_BUSY_TICK 1
#define FOX_TERMINAL_EVENT_SERIAL_DO_RETRY  2
#define FOX_TERMINAL_EVENT_POLL_TICK        3

typedef enum {
    ProbeResultOk,
    ProbeResultSerialBusy,
    ProbeResultNotFound,
} ProbeResult;

static void app_log_sanitize(char* buffer) {
    if(buffer == NULL) return;
    for(char* p = buffer; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if(c < 0x20 || c > 0x7E) {
            *p = '.';
        }
    }
}

void app_log(App* app, const char* fmt, ...) {
    if(app == NULL || app->log == NULL || fmt == NULL) return;

    char buffer[128];
    FuriString* tmp = furi_string_alloc();
    va_list args;
    va_start(args, fmt);
    furi_string_vprintf(tmp, fmt, args);
    va_end(args);
    snprintf(buffer, sizeof(buffer), "%s", furi_string_get_cstr(tmp));
    furi_string_free(tmp);
    buffer[sizeof(buffer) - 1] = '\0';

    app_log_sanitize(buffer);

    if(furi_string_size(app->log) > 0) furi_string_cat(app->log, "\n");
    furi_string_cat(app->log, buffer);

    if(furi_string_size(app->log) > FOX_TERMINAL_LOG_MAX_CHARS) {
        size_t excess = furi_string_size(app->log) - FOX_TERMINAL_LOG_MAX_CHARS;
        size_t cut = furi_string_search_char(app->log, '\n', excess);
        cut = (cut == FURI_STRING_FAILURE) ? excess : (cut + 1);
        furi_string_right(app->log, cut);
    }

    session_log_write_line(app, buffer);
}

bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms) {
    if(app == NULL || app->esp_at == NULL || expected == NULL) return false;

    EspAtMsg msg;
    uint32_t deadline = furi_get_tick() + timeout_ms;

    while(furi_get_tick() < deadline) {
        uint32_t remaining = deadline - furi_get_tick();
        if(!esp_at_receive(app->esp_at, &msg, remaining)) break;

        app_log(app, "%s", msg.line);
        if(strcmp(msg.line, expected) == 0) return true;
    }
    return false;
}

void app_show_terminal(App* app) {
    if(app == NULL) return;
    terminal_unpause(app);
    app->current_view = FoxTerminalViewTerminal;
    if(app->view_dispatcher != NULL) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewTerminal);
    }
}

void app_show_send_command(App* app, bool from_terminal) {
    if(app == NULL || app->text_input == NULL) return;
    app->send_from_terminal = from_terminal;
    snprintf(app->text_input_buffer, sizeof(app->text_input_buffer), "%s", app->last_command);
    text_input_set_header_text(app->text_input, "Send Command");
    app->current_view = FoxTerminalViewTextInput;
    if(app->view_dispatcher != NULL) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewTextInput);
    }
}

static void text_input_result_callback(void* context) {
    App* app = context;
    if(app == NULL) return;

    if(app->text_input_buffer[0] == '\0') {
        if(app->send_from_terminal) {
            app->current_view = FoxTerminalViewTerminal;
            if(app->view_dispatcher != NULL) {
                view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewTerminal);
            }
        } else {
            app_switch_to_main_menu(app);
        }
        return;
    }

    snprintf(app->last_command, sizeof(app->last_command), "%s", app->text_input_buffer);

    app_log(app, "> %s", app->text_input_buffer);
    if(app->esp_at != NULL) {
        esp_at_send(app->esp_at, app->text_input_buffer);
    } else {
        app_log(app, "(not sent - ESP32 not connected)");
    }

    app_show_terminal(app);
}

static ProbeResult app_probe_uart(App* app, size_t pin_index, size_t baud_index) {
    if(app == NULL || pin_index >= PIN_OPTION_COUNT ||
       baud_index >= (sizeof(baud_options) / sizeof(baud_options[0]))) {
        return ProbeResultNotFound;
    }

    app->esp_at = esp_at_alloc(pin_options[pin_index].serial_id, baud_options[baud_index]);
    if(app->esp_at == NULL) return ProbeResultSerialBusy;

    esp_at_send(app->esp_at, "info");
    bool ok = app_expect_line(app, "Fox ESP32 Firmware", 1500);

    if(!ok) {
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
        return ProbeResultNotFound;
    }

    app->pin_option_index = pin_index;
    app->baud_option_index = baud_index;
    return ProbeResultOk;
}

static void on_esp32_connected(App* app) {
    if(app == NULL) return;
    if(!app->session_log_open) {
        session_log_open(app);
        if(app->terminal_poll_timer != NULL) {
            furi_timer_start(app->terminal_poll_timer, furi_ms_to_ticks(100));
        }
    }
    app_switch_to_main_menu(app);
}

static void action_check_esp32(App* app) {
    if(app == NULL) return;
    message_view_show_detecting(app);

    bool any_busy = false;
    for(size_t i = 0; i < PIN_OPTION_COUNT; i++) {
        ProbeResult r = app_probe_uart(app, i, BAUD_OPTION_DEFAULT_INDEX);
        if(r == ProbeResultOk) {
            app->esp32_detected = true;
            app_log(app, "Fox ESP32 Firmware detected");
            app_log(
                app,
                "on %s @ %lu",
                pin_options[i].label,
                (unsigned long)baud_options[BAUD_OPTION_DEFAULT_INDEX]);
            on_esp32_connected(app);
            return;
        }
        if(r == ProbeResultSerialBusy) any_busy = true;
    }

    if(any_busy) {
        app->serial_busy_countdown = 3;
        message_view_show_serial_busy(app);
        if(app->serial_busy_timer != NULL) furi_timer_start(app->serial_busy_timer, 1000);
    } else {
        message_view_show_not_detected(app);
    }
}

void app_retry_detection(App* app) {
    if(app == NULL) return;
    action_check_esp32(app);
}

size_t app_pin_option_count(void) {
    return PIN_OPTION_COUNT;
}

const char* app_pin_option_label(size_t index) {
    if(index >= PIN_OPTION_COUNT) index = 0;
    return pin_options[index].label;
}

size_t app_baud_option_count(void) {
    return sizeof(baud_options) / sizeof(baud_options[0]);
}

uint32_t app_baud_option_value(size_t index) {
    size_t count = app_baud_option_count();
    if(index >= count) index = 0;
    return baud_options[index];
}

bool app_probe_uart_selected(App* app) {
    if(app == NULL) return false;
    message_view_show_detecting(app);

    ProbeResult r = app_probe_uart(app, app->pin_option_index, app->baud_option_index);
    if(r == ProbeResultOk) {
        app->esp32_detected = true;
        app_log(app, "Fox ESP32 Firmware detected");
        app_log(
            app,
            "on %s @ %lu",
            pin_options[app->pin_option_index].label,
            (unsigned long)baud_options[app->baud_option_index]);
        on_esp32_connected(app);
        return true;
    }

    if(r == ProbeResultSerialBusy) {
        app->serial_busy_countdown = 3;
        message_view_show_serial_busy(app);
        if(app->serial_busy_timer != NULL) furi_timer_start(app->serial_busy_timer, 1000);
        return false;
    }

    message_view_show_not_detected(app);
    return false;
}

static void fox_splash_done_cb(void* context) {
    App* app = context;
    if(app == NULL || app->view_dispatcher == NULL) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_TERMINAL_EVENT_SPLASH_DONE);
}

static void serial_busy_timer_cb(void* context) {
    App* app = context;
    if(app == NULL || app->view_dispatcher == NULL) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_TERMINAL_EVENT_SERIAL_BUSY_TICK);
}

static void serial_retry_timer_cb(void* context) {
    App* app = context;
    if(app == NULL || app->view_dispatcher == NULL) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_TERMINAL_EVENT_SERIAL_DO_RETRY);
}

static void terminal_poll_timer_cb(void* context) {
    App* app = context;
    if(app == NULL || app->view_dispatcher == NULL) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_TERMINAL_EVENT_POLL_TICK);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(app == NULL) return false;

    if(event == FOX_TERMINAL_EVENT_SPLASH_DONE) {
        action_check_esp32(app);
        return true;
    }
    if(event == FOX_TERMINAL_EVENT_SERIAL_BUSY_TICK) {
        if(app->serial_busy_countdown > 0) {
            app->serial_busy_countdown--;
            if(app->serial_busy_countdown == 0) {
                if(app->serial_busy_timer != NULL) furi_timer_stop(app->serial_busy_timer);
                app->message_view_serial_retrying = true;
            }
            if(app->message_view != NULL) {
                with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
            if(app->serial_busy_countdown == 0 && app->serial_retry_timer != NULL) {
                furi_timer_start(app->serial_retry_timer, 500);
            }
        }
        return true;
    }
    if(event == FOX_TERMINAL_EVENT_SERIAL_DO_RETRY) {
        bool found = false;
        for(size_t i = 0; i < PIN_OPTION_COUNT; i++) {
            ProbeResult r = app_probe_uart(app, i, BAUD_OPTION_DEFAULT_INDEX);
            if(r == ProbeResultOk) {
                app->esp32_detected = true;
                app->message_view_serial_busy     = false;
                app->message_view_serial_retrying = false;
                app_log(app, "Fox ESP32 Firmware detected");
                app_log(
                    app,
                    "on %s @ %lu",
                    pin_options[i].label,
                    (unsigned long)baud_options[BAUD_OPTION_DEFAULT_INDEX]);
                on_esp32_connected(app);
                found = true;
                break;
            }
        }
        if(!found) message_view_show_serial_retry_failed(app);
        return true;
    }
    if(event == FOX_TERMINAL_EVENT_POLL_TICK) {
        terminal_poll_tick(app);
        return true;
    }
    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;
    if(app == NULL || app->view_dispatcher == NULL) return true;

    switch(app->current_view) {
    case FoxTerminalViewMainMenu:
        view_dispatcher_stop(app->view_dispatcher);
        return true;

    case FoxTerminalViewTerminal:
        app_switch_to_main_menu(app);
        return true;

    case FoxTerminalViewLogList:
        log_list_scroll_stop(app);
        app_switch_to_main_menu(app);
        return true;

    case FoxTerminalViewLogContent:
        app_show_log_list(app);
        return true;

    case FoxTerminalViewTextInput:
        if(app->send_from_terminal) {
            app->current_view = FoxTerminalViewTerminal;
            view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewTerminal);
        } else {
            app_switch_to_main_menu(app);
        }
        return true;

    case FoxTerminalViewConnectSettings:
        app->current_view = FoxTerminalViewMessage;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewMessage);
        return true;

    default:
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
}

static App* app_alloc(bool skip_splash) {
    App* app = malloc(sizeof(App));
    if(app == NULL) return NULL;
    memset(app, 0, sizeof(App));

    app->log = furi_string_alloc();
    app->log_content = furi_string_alloc();
    app->baud_option_index = BAUD_OPTION_DEFAULT_INDEX;

    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    if(app->view_dispatcher == NULL || app->log == NULL || app->log_content == NULL) {
        if(app->log != NULL) furi_string_free(app->log);
        if(app->log_content != NULL) furi_string_free(app->log_content);
        if(app->view_dispatcher != NULL) view_dispatcher_free(app->view_dispatcher);
        if(app->gui != NULL) furi_record_close(RECORD_GUI);
        if(app->storage != NULL) furi_record_close(RECORD_STORAGE);
        free(app);
        return NULL;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->splash = fox_splash_alloc(&I_fox_64x64, 2000, 666, fox_splash_done_cb, app);

    app->message_view = message_view_alloc(app);
    app->connect_settings_view = connect_settings_view_alloc(app);
    app->main_menu_view = main_menu_view_alloc(app);
    app->terminal_view = terminal_view_alloc(app);
    app->log_list_view = log_list_view_alloc(app);
    app->log_content_view = log_view_screen_alloc(app);

    app->text_input = text_input_alloc();
    if(app->text_input != NULL) {
        text_input_set_result_callback(
            app->text_input,
            text_input_result_callback,
            app,
            app->text_input_buffer,
            sizeof(app->text_input_buffer),
            false);
    }

    if(app->splash != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewSplash, fox_splash_get_view(app->splash));
    }
    if(app->main_menu_view != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewMainMenu, app->main_menu_view);
    }
    if(app->message_view != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewMessage, app->message_view);
    }
    if(app->connect_settings_view != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewConnectSettings, app->connect_settings_view);
    }
    if(app->terminal_view != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewTerminal, app->terminal_view);
    }
    if(app->text_input != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewTextInput, text_input_get_view(app->text_input));
    }
    if(app->log_list_view != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewLogList, app->log_list_view);
    }
    if(app->log_content_view != NULL) {
        view_dispatcher_add_view(
            app->view_dispatcher, FoxTerminalViewLogContent, app->log_content_view);
    }

    if(app->gui != NULL) {
        view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    }

    app->serial_busy_timer  = furi_timer_alloc(serial_busy_timer_cb,  FuriTimerTypePeriodic, app);
    app->serial_retry_timer = furi_timer_alloc(serial_retry_timer_cb, FuriTimerTypeOnce,     app);
    app->terminal_poll_timer = furi_timer_alloc(terminal_poll_timer_cb, FuriTimerTypePeriodic, app);

    if(app->splash != NULL && !skip_splash) {
        app->current_view = FoxTerminalViewSplash;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewSplash);
        fox_splash_start(app->splash);
    } else {
        action_check_esp32(app);
    }

    return app;
}

static void app_free(App* app) {
    if(app == NULL) return;

    if(app->serial_busy_timer != NULL) {
        furi_timer_stop(app->serial_busy_timer);
        furi_timer_free(app->serial_busy_timer);
    }
    if(app->serial_retry_timer != NULL) {
        furi_timer_stop(app->serial_retry_timer);
        furi_timer_free(app->serial_retry_timer);
    }
    if(app->terminal_poll_timer != NULL) {
        furi_timer_stop(app->terminal_poll_timer);
        furi_timer_free(app->terminal_poll_timer);
    }

    if(app->terminal_paused_skipped_lines > 0) {
        app_log(
            app,
            "...(%lu line(s) skipped while paused)...",
            (unsigned long)app->terminal_paused_skipped_lines);
    }

    session_log_close(app);

    if(app->esp_at != NULL) esp_at_free(app->esp_at);

    if(app->view_dispatcher != NULL) {
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewSplash);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewMainMenu);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewMessage);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewConnectSettings);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewTerminal);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewTextInput);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewLogList);
        view_dispatcher_remove_view(app->view_dispatcher, FoxTerminalViewLogContent);
    }

    if(app->splash != NULL) fox_splash_free(app->splash);
    if(app->main_menu_view != NULL) main_menu_view_free(app->main_menu_view);
    if(app->terminal_view != NULL) terminal_view_free(app->terminal_view);
    if(app->message_view != NULL) message_view_free(app->message_view);
    if(app->connect_settings_view != NULL) connect_settings_view_free(app->connect_settings_view);
    if(app->log_list_view != NULL) log_list_view_free(app->log_list_view);
    if(app->log_content_view != NULL) log_view_screen_free(app->log_content_view);
    if(app->text_input != NULL) text_input_free(app->text_input);
    if(app->view_dispatcher != NULL) view_dispatcher_free(app->view_dispatcher);

    if(app->gui != NULL) furi_record_close(RECORD_GUI);
    if(app->storage != NULL) furi_record_close(RECORD_STORAGE);

    if(app->log != NULL) furi_string_free(app->log);
    if(app->log_content != NULL) furi_string_free(app->log_content);
    free(app);
}

int32_t fox_esp32_terminal_main(void* p) {
    bool skip_splash = (p != NULL && strcmp((const char*)p, "SKIPSPLASH") == 0);
    App* app = app_alloc(skip_splash);
    if(app == NULL) return -1;
    if(app->view_dispatcher != NULL) view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
