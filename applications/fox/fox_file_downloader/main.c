#include "app.h"
#include "fox_file_downloader_icons.h"
#include "wifi_menu.h"
#include "saved_wifi.h"
#include "settings_view.h"
#include "connect_settings.h"
#include "message_view.h"
#include "download.h"
#include "download_settings.h"
#include "download_progress_view.h"
#include "download_queue.h"
#include "url_download.h"
#include "catalog.h"
#include "apps_manager.h"
#include "github_repo.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

static void action_check_esp32(App* app);
static void main_render_menu(App* app);
static void main_menu_select(App* app, uint32_t index);
static MenuContext menu_parent_context(MenuContext ctx);
static void text_input_result_callback(void* context);
static void terminal_command_submitted(App* app);

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
#define FOX_DOWNLOADER_EVENT_SPLASH_DONE      0
#define FOX_DOWNLOADER_EVENT_SERIAL_BUSY_TICK 1
#define FOX_DOWNLOADER_EVENT_SERIAL_DO_RETRY  2

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

    if(furi_string_size(app->log) > FOX_TERMINAL_LOG_MAX_CHARS) {
        size_t excess = furi_string_size(app->log) - FOX_TERMINAL_LOG_MAX_CHARS;
        size_t cut = furi_string_search_char(app->log, '\n', excess);
        cut = (cut == FURI_STRING_FAILURE) ? excess : (cut + 1);
        furi_string_right(app->log, cut);
    }
}

void app_log_raw(App* app, const char* text) {
    if(furi_string_size(app->log) > 0) furi_string_cat(app->log, "\n");
    furi_string_cat(app->log, text);

    if(furi_string_size(app->log) > FOX_TERMINAL_LOG_MAX_CHARS) {
        size_t excess = furi_string_size(app->log) - FOX_TERMINAL_LOG_MAX_CHARS;
        size_t cut = furi_string_search_char(app->log, '\n', excess);
        cut = (cut == FURI_STRING_FAILURE) ? excess : (cut + 1);
        furi_string_right(app->log, cut);
    }
}

void app_render_log(App* app) {
    app->menu_return_context = app->menu_context;
    app->terminal_scroll = (size_t)-1;
    app->current_view = FoxDownloaderViewTerminal;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewTerminal);
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

void app_switch_to_menu(App* app, MenuContext ctx) {
    app->menu_context = ctx;
    switch(ctx) {
    case MenuContextMain:
        main_render_menu(app);
        break;
    case MenuContextWifi:
    case MenuContextWifiConnection:
    case MenuContextWifiRecon:
    case MenuContextWifiAttacks:
        wifi_render_menu(app, ctx);
        break;
    case MenuContextWifiSavedAction:
        wifi_saved_action_render_menu(app);
        break;
    case MenuContextCatalog:
        catalog_render_menu(app);
        break;
    }
    app->current_view = FoxDownloaderViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMenu);
}

void app_show_text_input(App* app, const char* header, TextInputPurpose purpose) {
    app_show_text_input_prefill(app, header, purpose, "");
}

static bool text_input_purpose_is_url(TextInputPurpose purpose) {
    switch(purpose) {
    case TextInputPurposeHttpDownloadUrl:
        return true;
    default:
        return false;
    }
}

void app_show_text_input_prefill(
    App* app,
    const char* header,
    TextInputPurpose purpose,
    const char* prefill) {
    app->menu_return_context = app->menu_context;
    app->text_input_purpose = purpose;

    bool prefill_empty = (prefill == NULL || prefill[0] == '\0');
    const char* effective_prefill =
        (prefill_empty && text_input_purpose_is_url(purpose)) ? "http://" : prefill;

    snprintf(
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        "%s",
        effective_prefill ? effective_prefill : "");

    text_input_set_header_text(app->text_input, header);
    app->current_view = FoxDownloaderViewTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewTextInput);
}

static void text_input_result_callback(void* context) {
    App* app = context;
    switch(app->text_input_purpose) {
    case TextInputPurposePassword:
        wifi_password_submitted(app);
        break;
    case TextInputPurposeBeaconSsids:
        wifi_beacon_custom_submitted(app);
        break;
    case TextInputPurposeHttpDownloadUrl:
        http_download_url_submitted(app);
        break;
    case TextInputPurposeTerminalCommand:
        terminal_command_submitted(app);
        break;
    case TextInputPurposeSavedWifiEditPassword:
        wifi_saved_edit_password_submitted(app);
        break;
    case TextInputPurposeGithubRepo:
        github_repo_submitted(app);
        break;
    default:
        break;
    }
}

void app_menu_item_callback(void* context, uint32_t index) {
    App* app = context;
    switch(app->menu_context) {
    case MenuContextMain:
        main_menu_select(app, index);
        break;
    case MenuContextWifi:
    case MenuContextWifiConnection:
    case MenuContextWifiRecon:
    case MenuContextWifiAttacks:
        wifi_menu_select(app, app->menu_context, index);
        break;
    case MenuContextWifiSavedAction:
        wifi_saved_action_select(app, index);
        break;
    case MenuContextCatalog:
        catalog_menu_select(app, index);
        break;
    }
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
        canvas, 64, TERMINAL_HEADER_H / 2, AlignCenter, AlignCenter, "TERMINAL");
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
        size_t n =
            wl->length < (TERMINAL_MEASURE_BUF_MAX - 1) ? wl->length : (TERMINAL_MEASURE_BUF_MAX - 1);
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

static void main_render_menu(App* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Fox File Downloader");
    if(!fox_wifi_status_read()) {
        submenu_add_item(app->submenu, "WiFi", MenuMainWifi, app_menu_item_callback, app);
    }
    submenu_add_item(
        app->submenu, "File Download (URL)", MenuMainFileDownloader, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "App Catalog", MenuMainCatalog, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Manage Apps", MenuMainMyApps, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "GitHub Repos", MenuMainGithub, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Settings", MenuMainSettings, app_menu_item_callback, app);
    submenu_add_item(app->submenu, "Terminal", MenuMainTerminal, app_menu_item_callback, app);
    if(app->expert_mode) {
        submenu_add_item(
            app->submenu,
            "Terminal Command",
            MenuMainTerminalCommand,
            app_menu_item_callback,
            app);
    }
    submenu_set_selected_item(app->submenu, app->main_menu_selected);
}

static void main_menu_select(App* app, uint32_t index) {
    app->main_menu_selected = index;
    switch((MenuMainIndex)index) {
    case MenuMainWifi: {
        esp_at_send(app->esp_at, "WIFIFOXPORTAL:STATUS");
        EspAtMsg portal_msg;
        bool portal_running = false;
        if(esp_at_receive(app->esp_at, &portal_msg, 1500)) {
            portal_running = strncmp(portal_msg.line, "FOXPORTAL:RUNNING:", 18) == 0;
        }
        if(portal_running) {
            message_view_show_portal_running(app);
            break;
        }
        app_switch_to_menu(app, MenuContextWifi);
        break;
    }
    case MenuMainCatalog:
        catalog_open(app);
        break;
    case MenuMainMyApps:
        my_apps_open(app);
        break;
    case MenuMainGithub:
        github_repo_open(app);
        break;
    case MenuMainFileDownloader:
        file_downloader_open(app);
        break;
    case MenuMainSettings:
        download_settings_view_reset(app);
        app->current_view = FoxDownloaderViewDownloadSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewDownloadSettings);
        with_view_model(app->download_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        break;
    case MenuMainTerminal:
        app_render_log(app);
        break;
    case MenuMainTerminalCommand:
        app_show_text_input(app, "Command", TextInputPurposeTerminalCommand);
        break;
    }
}

static void terminal_command_submitted(App* app) {
    if(app->text_input_buffer[0] == '\0') {
        app_log(app, "No command entered.");
        app_render_log(app);
        return;
    }

    esp_at_send(app->esp_at, app->text_input_buffer);

    app_log(app, "> %s", app->text_input_buffer);
    app_render_log(app);
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 10000)) {
        app_log(app, "%s", msg.line);
    } else {
        app_log(app, "No response.");
    }
    app_render_log(app);
}

static MenuContext menu_parent_context(MenuContext ctx) {
    switch(ctx) {
    case MenuContextWifiConnection:
    case MenuContextWifiRecon:
    case MenuContextWifiAttacks:
        return MenuContextWifi;
    case MenuContextWifiSavedAction:
        return MenuContextWifiConnection;
    default:
        return MenuContextMain;
    }
}

static ProbeResult app_probe_uart(App* app, size_t pin_index, size_t baud_index) {
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

static void app_goto_post_detect_menu(App* app) {
    app_expert_mode_refresh(app);
    if(app->launch_wifi_connection) {
        app->launch_wifi_connection = false;
        app_switch_to_menu(app, MenuContextWifiConnection);
    } else {
        app_switch_to_menu(app, MenuContextMain);
    }
}

static void action_check_esp32(App* app) {
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
            app_goto_post_detect_menu(app);
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
        app_goto_post_detect_menu(app);
        return true;
    }

    if(r == ProbeResultSerialBusy) {
        app->serial_busy_countdown = 3;
        message_view_show_serial_busy(app);
        furi_timer_start(app->serial_busy_timer, 1000);
        return false;
    }

    message_view_show_not_detected(app);
    return false;
}

static void fox_splash_done_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_DOWNLOADER_EVENT_SPLASH_DONE);
}

static void serial_busy_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_DOWNLOADER_EVENT_SERIAL_BUSY_TICK);
}

static void serial_retry_timer_cb(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FOX_DOWNLOADER_EVENT_SERIAL_DO_RETRY);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(event == FOX_DOWNLOADER_EVENT_SPLASH_DONE) {
        action_check_esp32(app);
        return true;
    }
    if(event == FOX_DOWNLOADER_EVENT_SKIP_WAIT_TIMEOUT) {
        // The "Not waiting for ESP32..." popup's 2.5s auto-dismiss fired -
        // the actual cancel is still running in the background and will
        // clean itself up via FOX_DOWNLOAD_EVENT_WORKER_DONE whenever the
        // ESP32 responds; this just stops making the user wait for it.
        download_progress_view_clear_wait_popups();
        app_switch_to_menu(app, app->menu_return_context);
        return true;
    }
    if(event == FOX_DOWNLOADER_EVENT_SERIAL_BUSY_TICK) {
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
    if(event == FOX_DOWNLOADER_EVENT_SERIAL_DO_RETRY) {
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
                app_goto_post_detect_menu(app);
                found = true;
                break;
            }
        }
        if(!found) message_view_show_serial_retry_failed(app);
        return true;
    }
    if(event == FOX_DOWNLOAD_EVENT_WORKER_DONE) {
        furi_timer_stop(app->download_progress_timer);
        bool ok;
        uint32_t bytes;
        char error[FOX_DOWNLOAD_ERR_MAX];
        furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
        ok = app->download_progress_ok;
        bytes = app->download_progress_bytes;
        snprintf(error, sizeof(error), "%s", app->download_progress_error);
        furi_mutex_release(app->download_progress_mutex);

        download_worker_free_if_done(app);

        if(app->download_purpose == DownloadPurposeFile && !app->download_confirmed) {
            download_unconfirmed_finished(app, ok, error);
            return true;
        }

        if(ok) {
            download_state_clear();

            if(app->download_purpose == DownloadPurposeCatalogPage) {
                catalog_show_app_list(app);
            } else if(app->download_purpose == DownloadPurposeGithubRepoInfo) {
                github_repo_info_loaded(app);
            } else if(app->download_purpose == DownloadPurposeGithubTree) {
                github_show_file_list(app);
            } else {
                // Stay on the progress view and let it show a "Complete"
                // screen (see current_phase() in download_progress_view.c,
                // which now checks download_progress_done/ok) instead of
                // jumping to the terminal/log screen - still logged for
                // the terminal's history in case it's checked later.
                app_log(
                    app,
                    "Downloaded %s (%lu KB) to %s",
                    app->download_found_name,
                    (unsigned long)(bytes / 1024),
                    app->download_path);
                download_progress_view_refresh(app->download_progress_view);
            }
        } else if(
            app->download_purpose == DownloadPurposeCatalogInstall &&
            strstr(error, "catalog API doesn't match firmware")) {
            catalog_show_disclaimer(
                app,
                "This app's catalog build doesn't match this firmware's API. May work on other firmware.",
                true);
        } else if(strcmp(error, "Stream protocol error") == 0) {
            // This specific error usually means the UART framing got
            // desynced, which is far more likely at a raised baud rate -
            // point at the fix instead of leaving it as a bare error.
            // Flipper-side only (see url_download.c) - no ESP32 firmware
            // change needed.
            if(app->download_settings.baud > FOX_DOWNLOAD_BAUD) {
                app_log(
                    app,
                    "Download failed: %s - %lu too high, retry at 115200 Baud.",
                    error,
                    (unsigned long)app->download_settings.baud);
            } else {
                app_log(
                    app,
                    "Download failed: %s If Retry Fails, Reset ESP32 and try again.",
                    error);
            }
            app_render_log(app);
        } else {
            app_log(app, "Download failed: %s", error);
            app_render_log(app);
        }
        return true;
    }
    return false;
}

static bool navigation_callback(void* context) {
    App* app = context;

    if(app->current_view == FoxDownloaderViewMenu) {
        if(app->menu_context == MenuContextMain) {
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
        if(app->menu_context == MenuContextWifiSavedAction && app->saved_wifi_count > 0) {
            app->current_view = FoxDownloaderViewSavedWifiList;
            view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewSavedWifiList);
            return true;
        }
        app_switch_to_menu(app, menu_parent_context(app->menu_context));
        return true;
    }

    if(app->current_view == FoxDownloaderViewTerminal ||
       app->current_view == FoxDownloaderViewNetworkList ||
       app->current_view == FoxDownloaderViewStationList ||
       app->current_view == FoxDownloaderViewSavedWifiList ||
       app->current_view == FoxDownloaderViewTextInput) {
        app_switch_to_menu(app, app->menu_return_context);
        return true;
    }

    if(app->current_view == FoxDownloaderViewDownloadSettings) {
        app_switch_to_menu(app, MenuContextMain);
        return true;
    }

    if(app->current_view == FoxDownloaderViewCatalogAppList) {
        app_switch_to_menu(app, MenuContextCatalog);
        return true;
    }

    if(app->current_view == FoxDownloaderViewMyAppsList ||
       app->current_view == FoxDownloaderViewGithubFileList) {
        app_switch_to_menu(app, MenuContextMain);
        return true;
    }

    if(app->current_view == FoxDownloaderViewConnectSettings) {
        app->current_view = FoxDownloaderViewMessage;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewMessage);
        return true;
    }

    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static App* app_alloc(bool skip_splash, bool wifi_connection_target) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->log = furi_string_alloc();
    app->pending_ssid = furi_string_alloc();
    app->baud_option_index = BAUD_OPTION_DEFAULT_INDEX;
    app->launch_wifi_connection = wifi_connection_target;

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->splash = fox_splash_alloc(&I_fox_64x64, 2000, 666, fox_splash_done_cb, app);

    app->submenu = submenu_alloc();
    app->message_view = message_view_alloc(app);

    app->text_input = text_input_alloc();
    text_input_set_result_callback(
        app->text_input,
        text_input_result_callback,
        app,
        app->text_input_buffer,
        sizeof(app->text_input_buffer),
        false);

    app->terminal_view = view_alloc();
    view_set_draw_callback(app->terminal_view, terminal_draw_cb);
    view_set_input_callback(app->terminal_view, terminal_input_cb);
    view_set_context(app->terminal_view, app);
    view_allocate_model(app->terminal_view, ViewModelTypeLocking, sizeof(uint8_t));
    s_terminal_view_app = app;

    app->network_list_view = wifi_network_list_view_alloc(app);
    app->station_list_view = wifi_station_list_view_alloc(app);
    app->saved_wifi_list_view = wifi_saved_list_view_alloc(app);
    app->connect_settings_view = connect_settings_view_alloc(app);

    app->download_progress_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    download_settings_load(app);
    app->download_settings_view = download_settings_view_alloc(app);
    app->download_found_view = download_found_view_alloc(app);
    app->download_progress_view = download_progress_view_alloc(app);
    app->download_resume_view = download_resume_view_alloc(app);
    download_queue_ensure_file();

    app->catalog_disclaimer_view = catalog_disclaimer_view_alloc(app);
    app->catalog_app_list_view = catalog_app_list_view_alloc(app);
    app->my_apps_list_view = my_apps_list_view_alloc(app);
    app->github_file_list_view = github_file_list_view_alloc(app);

    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewSplash, fox_splash_get_view(app->splash));
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewMessage, app->message_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewTerminal, app->terminal_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewNetworkList, app->network_list_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewStationList, app->station_list_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewSavedWifiList, app->saved_wifi_list_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewConnectSettings, app->connect_settings_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewDownloadSettings, app->download_settings_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewDownloadFound, app->download_found_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewDownloadProgress, app->download_progress_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewDownloadResume, app->download_resume_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewCatalogDisclaimer, app->catalog_disclaimer_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewCatalogAppList, app->catalog_app_list_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewMyAppsList, app->my_apps_list_view);
    view_dispatcher_add_view(
        app->view_dispatcher, FoxDownloaderViewGithubFileList, app->github_file_list_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->serial_busy_timer  = furi_timer_alloc(serial_busy_timer_cb,  FuriTimerTypePeriodic, app);
    app->serial_retry_timer = furi_timer_alloc(serial_retry_timer_cb, FuriTimerTypeOnce,     app);

    if(skip_splash) {
        action_check_esp32(app);
    } else {
        app->current_view = FoxDownloaderViewSplash;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxDownloaderViewSplash);
        fox_splash_start(app->splash);
    }

    return app;
}

static void app_free(App* app) {
    furi_timer_stop(app->serial_busy_timer);
    furi_timer_free(app->serial_busy_timer);
    furi_timer_stop(app->serial_retry_timer);
    furi_timer_free(app->serial_retry_timer);

    if(app->download_worker != NULL) {
        app->download_cancel_requested = true;
        furi_thread_join(app->download_worker);
        furi_thread_free(app->download_worker);
        app->download_worker = NULL;
    }

    if(app->esp_at != NULL) esp_at_free(app->esp_at);

    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewMessage);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewTerminal);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewNetworkList);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewStationList);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewSavedWifiList);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewConnectSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewDownloadSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewDownloadFound);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewDownloadProgress);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewDownloadResume);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewCatalogDisclaimer);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewCatalogAppList);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewMyAppsList);
    view_dispatcher_remove_view(app->view_dispatcher, FoxDownloaderViewGithubFileList);

    fox_splash_free(app->splash);
    submenu_free(app->submenu);
    view_free(app->terminal_view);
    s_terminal_view_app = NULL;
    wifi_network_list_view_free(app->network_list_view);
    wifi_station_list_view_free(app->station_list_view);
    wifi_saved_list_view_free(app->saved_wifi_list_view);
    connect_settings_view_free(app->connect_settings_view);
    download_settings_view_free(app->download_settings_view);
    download_found_view_free(app->download_found_view);
    download_progress_view_free(app->download_progress_view);
    download_resume_view_free(app->download_resume_view);
    catalog_disclaimer_view_free(app->catalog_disclaimer_view);
    catalog_app_list_view_free(app->catalog_app_list_view);
    my_apps_list_view_free(app->my_apps_list_view);
    github_file_list_view_free(app->github_file_list_view);
    catalog_free_buffers(app);
    github_free_buffers(app);
    my_apps_free_buffers(app);
    furi_mutex_free(app->download_progress_mutex);
    text_input_free(app->text_input);
    message_view_free(app->message_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->log);
    furi_string_free(app->pending_ssid);
    free(app);
}

int32_t fox_file_downloader_main(void* p) {
    const char* arg = (const char*)p;
    bool wifi_connection_target = (arg != NULL && strcmp(arg, "SKIPSPLASH_WIFICONN") == 0);
    bool skip_splash =
        wifi_connection_target || (arg != NULL && strcmp(arg, "SKIPSPLASH") == 0);
    App* app = app_alloc(skip_splash, wifi_connection_target);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
