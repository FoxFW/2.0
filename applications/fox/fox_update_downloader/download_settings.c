#include "app.h"
#include "fox_update_downloader_icons.h"
#include <gui/icon.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DL_SETTINGS_PATH UPDATER_DATA_DIR "/download_settings.txt"

static const uint32_t k_baud_choices[] = {115200U, 230400U, 460800U, 921600U};
#define BAUD_CHOICE_COUNT (sizeof(k_baud_choices) / sizeof(k_baud_choices[0]))

static const uint8_t k_retry_choices[] = {1, 2, 3, 5, 10, 15, 20};
#define RETRY_CHOICE_COUNT (sizeof(k_retry_choices) / sizeof(k_retry_choices[0]))

static const uint16_t k_timeout_choices[] = {5, 8, 10, 15, 20, 30, 45, 60};
#define TIMEOUT_CHOICE_COUNT (sizeof(k_timeout_choices) / sizeof(k_timeout_choices[0]))

typedef enum {
    DlSettingsRowBaud,
    DlSettingsRowRetries,
    DlSettingsRowTimeout,
    DlSettingsRowAutoLower,
    DlSettingsRowCount,
} DlSettingsRow;

#define DL_ROW_X   4
#define DL_ROW_W   120
#define DL_ROW_R   3
#define DL_ROW_TOP 10
#define DL_ROW_H   12
#define DL_ROW_GAP 1

static UpdaterApp* s_download_settings_app = NULL;

static size_t index_of_u32(const uint32_t* arr, size_t count, uint32_t value) {
    for(size_t i = 0; i < count; i++) {
        if(arr[i] == value) return i;
    }
    return 0;
}

static size_t index_of_u8(const uint8_t* arr, size_t count, uint8_t value) {
    for(size_t i = 0; i < count; i++) {
        if(arr[i] == value) return i;
    }
    return 0;
}

static size_t index_of_u16(const uint16_t* arr, size_t count, uint16_t value) {
    for(size_t i = 0; i < count; i++) {
        if(arr[i] == value) return i;
    }
    return 0;
}

static bool contains_u32(const uint32_t* arr, size_t count, uint32_t value) {
    for(size_t i = 0; i < count; i++) {
        if(arr[i] == value) return true;
    }
    return false;
}

static bool contains_u8(const uint8_t* arr, size_t count, uint8_t value) {
    for(size_t i = 0; i < count; i++) {
        if(arr[i] == value) return true;
    }
    return false;
}

static bool contains_u16(const uint16_t* arr, size_t count, uint16_t value) {
    for(size_t i = 0; i < count; i++) {
        if(arr[i] == value) return true;
    }
    return false;
}

void updater_settings_load(UpdaterApp* app) {
    app->settings.baud = UPDATER_BAUD;
    app->settings.retry_attempts = 3;
    app->settings.timeout_sec = 10;
    app->settings.auto_lower_baud = false;

    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, DL_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[160] = {0};
        uint16_t got = storage_file_read(f, buf, sizeof(buf) - 1);
        buf[got] = '\0';

        char* p = strstr(buf, "baud=");
        if(p) app->settings.baud = (uint32_t)strtoul(p + 5, NULL, 10);

        p = strstr(buf, "retries=");
        if(p) app->settings.retry_attempts = (uint8_t)strtoul(p + 8, NULL, 10);

        p = strstr(buf, "timeout=");
        if(p) app->settings.timeout_sec = (uint16_t)strtoul(p + 8, NULL, 10);

        p = strstr(buf, "autolower=");
        if(p) app->settings.auto_lower_baud = strtoul(p + 10, NULL, 10) != 0;
    }
    storage_file_close(f);
    storage_file_free(f);

    if(!contains_u32(k_baud_choices, BAUD_CHOICE_COUNT, app->settings.baud)) {
        app->settings.baud = UPDATER_BAUD;
    }
    if(!contains_u8(k_retry_choices, RETRY_CHOICE_COUNT, app->settings.retry_attempts)) {
        app->settings.retry_attempts = 3;
    }
    if(!contains_u16(k_timeout_choices, TIMEOUT_CHOICE_COUNT, app->settings.timeout_sec)) {
        app->settings.timeout_sec = 10;
    }
}

void updater_settings_save(UpdaterApp* app) {
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, DL_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[160];
        int len = snprintf(
            buf,
            sizeof(buf),
            "baud=%lu\nretries=%u\ntimeout=%u\nautolower=%u\n",
            (unsigned long)app->settings.baud,
            (unsigned)app->settings.retry_attempts,
            (unsigned)app->settings.timeout_sec,
            (unsigned)(app->settings.auto_lower_baud ? 1 : 0));
        storage_file_write(f, buf, (uint16_t)len);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void download_settings_draw_row(
    Canvas* canvas, int32_t y, int32_t h, bool selected, const char* row_text) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, DL_ROW_X, y, DL_ROW_W, h, DL_ROW_R);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, DL_ROW_X, y, DL_ROW_W, h, DL_ROW_R);
    }

    int32_t text_y = y + h / 2;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, row_text);
    canvas_draw_str_aligned(canvas, DL_ROW_X + 6, text_y, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, DL_ROW_X + DL_ROW_W - 6, text_y, AlignRight, AlignCenter, ">");

    canvas_set_color(canvas, ColorBlack);
}

static void download_settings_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    UpdaterApp* app = s_download_settings_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Download Settings");

    int32_t y0 = DL_ROW_TOP;
    int32_t y1 = y0 + DL_ROW_H + DL_ROW_GAP;
    int32_t y2 = y1 + DL_ROW_H + DL_ROW_GAP;
    int32_t y3 = y2 + DL_ROW_H + DL_ROW_GAP;

    char text[32];

    snprintf(text, sizeof(text), "Baud: %lu", (unsigned long)app->settings.baud);
    download_settings_draw_row(canvas, y0, DL_ROW_H, app->download_settings_selected == DlSettingsRowBaud, text);

    snprintf(text, sizeof(text), "Retries: %u", (unsigned)app->settings.retry_attempts);
    download_settings_draw_row(canvas, y1, DL_ROW_H, app->download_settings_selected == DlSettingsRowRetries, text);

    snprintf(text, sizeof(text), "Timeout: %us", (unsigned)app->settings.timeout_sec);
    download_settings_draw_row(canvas, y2, DL_ROW_H, app->download_settings_selected == DlSettingsRowTimeout, text);

    snprintf(text, sizeof(text), "Auto Lower Baud: %s", app->settings.auto_lower_baud ? "Yes" : "No");
    download_settings_draw_row(canvas, y3, DL_ROW_H, app->download_settings_selected == DlSettingsRowAutoLower, text);
}

static void download_settings_cycle_baud(UpdaterApp* app, int delta) {
    size_t idx = index_of_u32(k_baud_choices, BAUD_CHOICE_COUNT, app->settings.baud);
    idx = (delta > 0) ? (idx + 1) % BAUD_CHOICE_COUNT :
                         ((idx == 0) ? BAUD_CHOICE_COUNT - 1 : idx - 1);
    app->settings.baud = k_baud_choices[idx];
}

static void download_settings_cycle_retries(UpdaterApp* app, int delta) {
    size_t idx = index_of_u8(k_retry_choices, RETRY_CHOICE_COUNT, app->settings.retry_attempts);
    idx = (delta > 0) ? (idx + 1) % RETRY_CHOICE_COUNT :
                         ((idx == 0) ? RETRY_CHOICE_COUNT - 1 : idx - 1);
    app->settings.retry_attempts = k_retry_choices[idx];
}

static void download_settings_cycle_timeout(UpdaterApp* app, int delta) {
    size_t idx = index_of_u16(k_timeout_choices, TIMEOUT_CHOICE_COUNT, app->settings.timeout_sec);
    idx = (delta > 0) ? (idx + 1) % TIMEOUT_CHOICE_COUNT :
                         ((idx == 0) ? TIMEOUT_CHOICE_COUNT - 1 : idx - 1);
    app->settings.timeout_sec = k_timeout_choices[idx];
}

static bool download_settings_input_cb(InputEvent* event, void* context) {
    UpdaterApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        app->download_settings_selected = (app->download_settings_selected == 0) ?
                                               (uint8_t)(DlSettingsRowCount - 1) :
                                               (uint8_t)(app->download_settings_selected - 1);
        with_view_model(app->download_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->download_settings_selected =
            (uint8_t)((app->download_settings_selected + 1) % DlSettingsRowCount);
        with_view_model(app->download_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyLeft:
    case InputKeyRight: {
        int delta = (event->key == InputKeyRight) ? 1 : -1;
        switch(app->download_settings_selected) {
        case DlSettingsRowBaud:
            download_settings_cycle_baud(app, delta);
            break;
        case DlSettingsRowRetries:
            download_settings_cycle_retries(app, delta);
            break;
        case DlSettingsRowTimeout:
            download_settings_cycle_timeout(app, delta);
            break;
        case DlSettingsRowAutoLower:
            app->settings.auto_lower_baud = !app->settings.auto_lower_baud;
            break;
        default:
            break;
        }
        updater_settings_save(app);
        with_view_model(app->download_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    }
    case InputKeyOk:
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* download_settings_view_alloc(UpdaterApp* app) {
    s_download_settings_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, download_settings_draw_cb);
    view_set_input_callback(view, download_settings_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void download_settings_view_free(View* view) {
    s_download_settings_app = NULL;
    view_free(view);
}

void download_settings_view_reset(UpdaterApp* app) {
    app->download_settings_selected = DlSettingsRowBaud;
}
