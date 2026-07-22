#include "settings_view.h"

#include <stdio.h>
#include <string.h>
#include <storage/storage.h>

static App* s_settings_view_app = NULL;

#define SETTINGS_ROW_COUNT 2
#define SETTINGS_ROW_TOP   18
#define SETTINGS_ROW_H     16
#define SETTINGS_ROW_GAP   4
#define SETTINGS_BOX_X     10
#define SETTINGS_BOX_W     108

#define EXPERT_MODE_DIR  "/ext/apps_data/fox_esp32_commander"
#define EXPERT_MODE_PATH "/ext/apps_data/fox_esp32_commander/expert_mode.txt"

void app_expert_mode_load(App* app) {
    app->expert_mode = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, EXPERT_MODE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[1] = {0};
        if(storage_file_read(file, buf, 1) == 1) {
            app->expert_mode = (buf[0] == '1');
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void expert_mode_save(bool enabled) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, EXPERT_MODE_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, EXPERT_MODE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* v = enabled ? "1" : "0";
        storage_file_write(file, v, 1);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void settings_draw_row(
    Canvas* canvas,
    int32_t y,
    const char* label,
    bool value,
    bool selected) {
    char row_text[28];
    snprintf(row_text, sizeof(row_text), "%s: %s", label, value ? "ON" : "OFF");

    int32_t text_y = y + SETTINGS_ROW_H / 2;

    if(selected) {
        canvas_draw_rbox(canvas, SETTINGS_BOX_X, y, SETTINGS_BOX_W, SETTINGS_ROW_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, row_text);
        canvas_draw_str_aligned(
            canvas, SETTINGS_BOX_X + 6, text_y, AlignLeft, AlignCenter, "<");
        canvas_draw_str_aligned(
            canvas, SETTINGS_BOX_X + SETTINGS_BOX_W - 6, text_y, AlignRight, AlignCenter, ">");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, row_text);
    }
}

static void settings_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_settings_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Settings");

    int32_t y0 = SETTINGS_ROW_TOP;
    int32_t y1 = y0 + SETTINGS_ROW_H + SETTINGS_ROW_GAP;

    settings_draw_row(canvas, y0, "Attacks", app->attacks_enabled, app->settings_selected == 0);
    settings_draw_row(canvas, y1, "Expert Mode", app->expert_mode, app->settings_selected == 1);

    canvas_set_font(canvas, FontSecondary);
    const char* caption = app->settings_selected == 0 ?
                               "Applies immediately." :
                               "Adds Terminal Command to the main menu.";
    canvas_draw_str_aligned(canvas, 64, y1 + SETTINGS_ROW_H + 11, AlignCenter, AlignCenter, caption);
}

static void settings_apply_attacks(App* app) {
    with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
    esp_at_send(app->esp_at, app->attacks_enabled ? "SETTINGS:ATTACKS:ON" : "SETTINGS:ATTACKS:OFF");
    EspAtMsg msg;
    esp_at_receive(app->esp_at, &msg, 1500);
}

static void settings_apply_expert_mode(App* app) {
    with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
    expert_mode_save(app->expert_mode);
}

static bool settings_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        app->settings_selected = 0;
        with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->settings_selected = 1;
        with_view_model(app->settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyLeft:
    case InputKeyRight:
        if(app->settings_selected == 0) {
            app->attacks_enabled = !app->attacks_enabled;
            settings_apply_attacks(app);
        } else {
            app->expert_mode = !app->expert_mode;
            settings_apply_expert_mode(app);
        }
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* settings_view_alloc(App* app) {
    s_settings_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, settings_draw_cb);
    view_set_input_callback(view, settings_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void settings_view_free(View* view) {
    s_settings_view_app = NULL;
    view_free(view);
}

void settings_view_refresh(App* app) {
    esp_at_send(app->esp_at, "SETTINGS");
    EspAtMsg msg;
    if(esp_at_receive(app->esp_at, &msg, 1500)) {
        app->attacks_enabled = (strcmp(msg.line, "ATTACKS:ON") == 0);
    }
    /* expert_mode is deliberately untouched here - see settings_view.h's
       comment on this function. */
}
