#include "app.h"
#include "fox_ai_chat_icons.h"
#include "ai_chat_menu.h"
#include "message_view.h"
#include "progress_view.h"
#include "message_limit_view.h"
#include "gpio_remap_compat.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include <loader/loader.h>
#include <storage/storage.h>

static void action_check_esp32(App* app);
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
#define AI_CHAT_BAUD_RATE 115200

#define AI_CHAT_TERMINAL_LOG_MAX_CHARS 4000
#define AI_CHAT_EVENT_SPLASH_DONE       0
#define AI_CHAT_EVENT_SERIAL_BUSY_TICK  1
#define AI_CHAT_EVENT_SERIAL_DO_RETRY   2
#define AI_CHAT_EVENT_MESSAGE_LIMIT_TICK 3

typedef enum {
    ProbeResultOk,
    ProbeResultSerialBusy,
    ProbeResultNotFound,
} ProbeResult;

void app_log(App* app, const char* fmt, ...) {
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if(furi_string_size(app->log) > 0) furi_string_cat(app->log, "\n");
    furi_string_cat(app->log, buffer);

    if(furi_string_size(app->log) > AI_CHAT_TERMINAL_LOG_MAX_CHARS) {
        size_t excess = furi_string_size(app->log) - AI_CHAT_TERMINAL_LOG_MAX_CHARS;
        size_t cut = furi_string_search_char(app->log, '\n', excess);
        cut = (cut == FURI_STRING_FAILURE) ? excess : (cut + 1);
        furi_string_right(app->log, cut);
    }
}

void app_render_log(App* app) {
    app->terminal_scroll = (size_t)-1;
    app->current_view = AiChatViewTerminal;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewTerminal);
}

bool app_expect_line(App* app, const char* expected, uint32_t timeout_ms) {
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

void app_switch_to_menu(App* app) {
    ai_chat_render_menu(app);
    app->current_view = AiChatViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMenu);
}

void app_show_text_input(App* app, const char* header, TextInputPurpose purpose) {
    app->text_input_purpose = purpose;
    app->text_input_buffer[0] = '\0';
    text_input_set_result_callback(
        app->text_input,
        text_input_result_callback,
        app,
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        true);
    text_input_set_header_text(app->text_input, header);
    app->current_view = AiChatViewTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewTextInput);
}

void app_show_text_input_restore(App* app, const char* header, TextInputPurpose purpose) {
    app->text_input_purpose = purpose;
    text_input_set_result_callback(
        app->text_input,
        text_input_result_callback,
        app,
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        false);
    text_input_set_header_text(app->text_input, header);
    app->current_view = AiChatViewTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewTextInput);
}

static void text_input_result_callback(void* context) {
    App* app = context;
    switch(app->text_input_purpose) {
    case TextInputPurposeChatMessage:
        chat_message_submitted(app);
        break;
    default:
        break;
    }
}

void app_menu_item_callback(void* context, uint32_t index) {
    App* app = context;
    ai_chat_menu_select(app, index);
}

static App* s_terminal_view_app = NULL;

#define TERMINAL_HEADER_H          10
#define TERMINAL_MAX_WRAPPED_LINES 256
#define TERMINAL_MEASURE_BUF_MAX   136

static size_t terminal_chars_per_line(Canvas* canvas, int32_t max_width) {
    uint16_t w = canvas_string_width(canvas, "W");
    if(w == 0) w = 6;
    size_t n = (size_t)(max_width / w);
    return n < 4 ? 4 : n;
}

typedef struct {
    uint16_t offset;
    uint16_t length;
} TerminalWrapLine;

static size_t terminal_wrap_log(
    const char* text,
    size_t text_len,
    size_t chars_per_line,
    TerminalWrapLine* out,
    size_t out_capacity) {
    size_t count = 0;
    size_t line_start = 0;

    while(line_start <= text_len && count < out_capacity) {
        size_t line_end = line_start;
        while(line_end < text_len && text[line_end] != '\n') line_end++;

        if(line_end == line_start) {
            out[count].offset = (uint16_t)line_start;
            out[count].length = 0;
            count++;
        } else {
            size_t pos = line_start;
            while(pos < line_end && count < out_capacity) {
                size_t remaining = line_end - pos;
                size_t take = remaining < chars_per_line ? remaining : chars_per_line;
                size_t chunk_end = pos + take;

                if(take == chars_per_line && chunk_end < line_end) {
                    size_t min_break = pos + (chars_per_line / 3);
                    for(size_t i = chunk_end; i > pos && i > min_break; i--) {
                        if(text[i - 1] == ' ') {
                            chunk_end = i - 1;
                            break;
                        }
                    }
                }

                out[count].offset = (uint16_t)pos;
                out[count].length = (uint16_t)(chunk_end - pos);
                count++;
                pos = chunk_end;
                if(pos < line_end && text[pos] == ' ') pos++;
            }
        }

        if(line_end == text_len) break;
        line_start = line_end + 1;
    }

    return count;
}

static void terminal_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_terminal_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, TERMINAL_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, TERMINAL_HEADER_H / 2, AlignCenter, AlignCenter, "AI CHAT");
    canvas_set_color(canvas, ColorBlack);

    const char* text = furi_string_get_cstr(app->log);
    size_t text_len = furi_string_size(app->log);

    int32_t max_width = 122;
    size_t chars_per_line = terminal_chars_per_line(canvas, max_width);

    static TerminalWrapLine lines[TERMINAL_MAX_WRAPPED_LINES];
    size_t total =
        terminal_wrap_log(text, text_len, chars_per_line, lines, TERMINAL_MAX_WRAPPED_LINES);

    size_t line_height = canvas_current_font_height(canvas);
    if(line_height == 0) line_height = 8;
    size_t content_top = TERMINAL_HEADER_H + 1;
    size_t content_height = 64 - content_top;
    size_t visible_rows = content_height / line_height;
    if(visible_rows == 0) visible_rows = 1;

    size_t max_scroll = total > visible_rows ? total - visible_rows : 0;
    if(app->terminal_scroll > max_scroll) app->terminal_scroll = max_scroll;

    for(size_t row = 0; row < visible_rows && (app->terminal_scroll + row) < total; row++) {
        const TerminalWrapLine* wl = &lines[app->terminal_scroll + row];
        char buf[TERMINAL_MEASURE_BUF_MAX];
        size_t n = wl->length < (TERMINAL_MEASURE_BUF_MAX - 1) ? wl->length :
                                                                  (TERMINAL_MEASURE_BUF_MAX - 1);
        memcpy(buf, text + wl->offset, n);
        buf[n] = '\0';
        int32_t y = (int32_t)(content_top + row * line_height + line_height - 1);
        canvas_draw_str(canvas, 2, y, buf);
    }

    if(total > visible_rows) {
        int32_t bar_x = 126;
        int32_t bar_top = (int32_t)content_top;
        int32_t bar_h = (int32_t)content_height;
        canvas_draw_line(canvas, bar_x, bar_top, bar_x, bar_top + bar_h);

        int32_t dot_h = bar_h * (int32_t)visible_rows / (int32_t)total;
        if(dot_h < 3) dot_h = 3;
        int32_t dot_y =
            bar_top + (bar_h - dot_h) * (int32_t)app->terminal_scroll / (int32_t)max_scroll;
        canvas_draw_box(canvas, bar_x - 1, dot_y, 3, dot_h);
    }
}

static bool terminal_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->terminal_scroll > 0) app->terminal_scroll--;
        with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->terminal_scroll++;
        with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false;
    default:
        return false;
    }
}

static ProbeResult app_probe_uart(App* app, size_t pin_index) {
    app->esp_at = esp_at_alloc(pin_options[pin_index].serial_id, AI_CHAT_BAUD_RATE);
    if(app->esp_at == NULL) return ProbeResultSerialBusy;

    esp_at_send(app->esp_at, "info");
    bool ok = app_expect_line(app, "Fox ESP32 Firmware", 1500);

    if(!ok) {
        esp_at_free(app->esp_at);
        app->esp_at = NULL;
        return ProbeResultNotFound;
    }

    app->pin_option_index = pin_index;

    GpioRemapSettings gpio_remap = {.esp32_uart_channel = (uint8_t)pin_index};
    gpio_remap_settings_save(&gpio_remap);

    return ProbeResultOk;
}

static void action_check_wifi(App* app) {
    esp_at_send(app->esp_at, "[WIFI/STATUS]");
    EspAtMsg msg;
    bool connected = esp_at_receive(app->esp_at, &msg, 5000) &&
                     strcmp(msg.line, "[WIFI/STATUS/SUCCESS]true") == 0;
    if(connected) {
        app->message_view_wifi_not_connected = false;
        app_switch_to_menu(app);
    } else {
        message_view_show_wifi_not_connected(app);
    }
}

static void action_check_esp32(App* app) {
    message_view_show_detecting(app);

    bool any_busy = false;
    for(size_t i = 0; i < PIN_OPTION_COUNT; i++) {
        ProbeResult r = app_probe_uart(app, i);
        if(r == ProbeResultOk) {
            app->esp32_detected = true;
            app_log(app, "Fox ESP32 Firmware detected");
            app_log(app, "on %s @ %u", pin_options[i].label, (unsigned)AI_CHAT_BAUD_RATE);
            action_check_wifi(app);
            return;
        }
        if(r == ProbeResultSerialBusy) any_busy = true;
    }

    if(any_busy) {
        app->serial_busy_countdown = 3;
        message_view_show_serial_busy(app);
        furi_timer_start(app->serial_busy_timer, 1000);
    } else {
        message_view_show_not_detected(app);
    }
}

void app_retry_detection(App* app) {
    action_check_esp32(app);
}

void app_launch_commander(App* app) {
    app->launch_commander = true;
    view_dispatcher_stop(app->view_dispatcher);
}

static void fox_splash_done_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AI_CHAT_EVENT_SPLASH_DONE);
}

static void serial_busy_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AI_CHAT_EVENT_SERIAL_BUSY_TICK);
}

static void serial_retry_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AI_CHAT_EVENT_SERIAL_DO_RETRY);
}

static void message_limit_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AI_CHAT_EVENT_MESSAGE_LIMIT_TICK);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(event == AI_CHAT_EVENT_SPLASH_DONE) {
        action_check_esp32(app);
        return true;
    }
    if(event == AI_CHAT_EVENT_SERIAL_BUSY_TICK) {
        if(app->serial_busy_countdown > 0) {
            app->serial_busy_countdown--;
            if(app->serial_busy_countdown == 0) {
                furi_timer_stop(app->serial_busy_timer);
                app->message_view_serial_retrying = true;
            }
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
            if(app->serial_busy_countdown == 0) {
                furi_timer_start(app->serial_retry_timer, 500);
            }
        }
        return true;
    }
    if(event == AI_CHAT_EVENT_SERIAL_DO_RETRY) {
        bool found = false;
        for(size_t i = 0; i < PIN_OPTION_COUNT; i++) {
            ProbeResult r = app_probe_uart(app, i);
            if(r == ProbeResultOk) {
                app->esp32_detected = true;
                app->message_view_serial_busy = false;
                app->message_view_serial_retrying = false;
                app_log(app, "Fox ESP32 Firmware detected");
                app_log(app, "on %s @ %u", pin_options[i].label, (unsigned)AI_CHAT_BAUD_RATE);
                action_check_wifi(app);
                found = true;
                break;
            }
        }
        if(!found) message_view_show_serial_retry_failed(app);
        return true;
    }
    if(event == AI_CHAT_EVENT_MESSAGE_LIMIT_TICK) {
        message_limit_view_tick(app);
        return true;
    }
    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;

    if(app->current_view == AiChatViewMenu) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(app->current_view == AiChatViewTerminal || app->current_view == AiChatViewTextInput) {
        app_switch_to_menu(app);
        return true;
    }

    if(app->current_view == AiChatViewMessageLimit) {
        furi_timer_stop(app->message_limit_timer);
        app_switch_to_menu(app);
        return true;
    }

    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static App* app_alloc(bool skip_splash) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->log = furi_string_alloc();

    GpioRemapSettings gpio_remap;
    gpio_remap_settings_load(&gpio_remap);
    if(gpio_remap.esp32_uart_channel < PIN_OPTION_COUNT) {
        app->pin_option_index = gpio_remap.esp32_uart_channel;
    }

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->splash = fox_splash_alloc(&I_fox_64x64, 2000, 666, fox_splash_done_cb, app);

    app->submenu = submenu_alloc();
    app->message_view = message_view_alloc(app);
    app->progress_view = progress_view_alloc(app);
    app->message_limit_view = message_limit_view_alloc(app);

    app->text_input = text_input_alloc();
    text_input_set_result_callback(
        app->text_input,
        text_input_result_callback,
        app,
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        true);

    app->terminal_view = view_alloc();
    view_set_draw_callback(app->terminal_view, terminal_draw_cb);
    view_set_input_callback(app->terminal_view, terminal_input_cb);
    view_set_context(app->terminal_view, app);
    view_allocate_model(app->terminal_view, ViewModelTypeLocking, sizeof(uint8_t));
    s_terminal_view_app = app;

    view_dispatcher_add_view(
        app->view_dispatcher, AiChatViewSplash, fox_splash_get_view(app->splash));
    view_dispatcher_add_view(
        app->view_dispatcher, AiChatViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, AiChatViewMessage, app->message_view);
    view_dispatcher_add_view(app->view_dispatcher, AiChatViewTerminal, app->terminal_view);
    view_dispatcher_add_view(
        app->view_dispatcher, AiChatViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->view_dispatcher, AiChatViewProgress, app->progress_view);
    view_dispatcher_add_view(
        app->view_dispatcher, AiChatViewMessageLimit, app->message_limit_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->serial_busy_timer = furi_timer_alloc(serial_busy_timer_cb, FuriTimerTypePeriodic, app);
    app->serial_retry_timer = furi_timer_alloc(serial_retry_timer_cb, FuriTimerTypeOnce, app);
    app->message_limit_timer =
        furi_timer_alloc(message_limit_timer_cb, FuriTimerTypePeriodic, app);

    if(skip_splash) {
        action_check_esp32(app);
    } else {
        app->current_view = AiChatViewSplash;
        view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewSplash);
        fox_splash_start(app->splash);
    }

    return app;
}

static void app_free(App* app) {
    furi_timer_stop(app->serial_busy_timer);
    furi_timer_free(app->serial_busy_timer);
    furi_timer_stop(app->serial_retry_timer);
    furi_timer_free(app->serial_retry_timer);
    furi_timer_stop(app->message_limit_timer);
    furi_timer_free(app->message_limit_timer);

    if(app->esp_at != NULL) esp_at_free(app->esp_at);

    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewMessage);
    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewTerminal);
    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewProgress);
    view_dispatcher_remove_view(app->view_dispatcher, AiChatViewMessageLimit);

    fox_splash_free(app->splash);
    submenu_free(app->submenu);
    view_free(app->terminal_view);
    s_terminal_view_app = NULL;
    text_input_free(app->text_input);
    message_view_free(app->message_view);
    progress_view_free(app->progress_view);
    message_limit_view_free(app->message_limit_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->log);
    free(app);
}

int32_t fox_ai_chat_main(void* p) {
    bool skip_splash = (p != NULL && strcmp((const char*)p, "SKIPSPLASH") == 0);
    App* app = app_alloc(skip_splash);
    view_dispatcher_run(app->view_dispatcher);

    bool launch_commander = app->launch_commander;

    app_free(app);

    if(launch_commander) {
        Loader* loader = furi_record_open(RECORD_LOADER);
        loader_enqueue_launch(
            loader,
            EXT_PATH("apps/Fox/ESP32/fox_esp32_commander.fap"),
            NULL,
            LoaderDeferredLaunchFlagNone);
        furi_record_close(RECORD_LOADER);
    }

    return 0;
}
