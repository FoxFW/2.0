#include "connect_settings.h"

#include <stdio.h>

static App* s_connect_settings_app = NULL;

typedef enum {
    ConnectSettingsRowPins,
    ConnectSettingsRowBaud,
    ConnectSettingsRowStart,
    ConnectSettingsRowCount,
} ConnectSettingsRow;

#define CONNECT_ROW_TOP 14
#define CONNECT_ROW_H   15
#define CONNECT_BOX_X   14
#define CONNECT_BOX_W   100

static void connect_settings_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_connect_settings_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Connect Settings");

    for(size_t row = 0; row < ConnectSettingsRowCount; row++) {
        int32_t y = CONNECT_ROW_TOP + (int32_t)row * CONNECT_ROW_H;
        bool selected = (row == app->connect_settings_selected);
        bool adjustable = true;

        char row_text[32];
        switch((ConnectSettingsRow)row) {
        case ConnectSettingsRowPins:
            snprintf(
                row_text, sizeof(row_text), "Pins: %s", app_pin_option_label(app->pin_option_index));
            break;
        case ConnectSettingsRowBaud:
            snprintf(
                row_text,
                sizeof(row_text),
                "Baud: %lu",
                (unsigned long)app_baud_option_value(app->baud_option_index));
            break;
        case ConnectSettingsRowStart:
        default:
            snprintf(row_text, sizeof(row_text), "Retry");
            adjustable = false;
            break;
        }

        if(selected) {
            canvas_draw_rbox(canvas, CONNECT_BOX_X, y, CONNECT_BOX_W, CONNECT_ROW_H - 2, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, CONNECT_BOX_X, y, CONNECT_BOX_W, CONNECT_ROW_H - 2, 3);
        }

        int32_t text_y = y + (CONNECT_ROW_H - 2) / 2;
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, row_text);
        if(adjustable) {
            canvas_draw_str_aligned(canvas, CONNECT_BOX_X + 6, text_y, AlignLeft, AlignCenter, "<");
            canvas_draw_str_aligned(
                canvas, CONNECT_BOX_X + CONNECT_BOX_W - 6, text_y, AlignRight, AlignCenter, ">");
        }

        canvas_set_color(canvas, ColorBlack);
    }
}

static void connect_settings_cycle_pin(App* app, int delta) {
    size_t count = app_pin_option_count();
    if(count <= 1) return;
    size_t idx = app->pin_option_index;
    idx = (delta > 0) ? (idx + 1) % count : ((idx == 0) ? count - 1 : idx - 1);
    app->pin_option_index = idx;
}

static void connect_settings_cycle_baud(App* app, int delta) {
    size_t count = app_baud_option_count();
    if(count <= 1) return;
    size_t idx = app->baud_option_index;
    idx = (delta > 0) ? (idx + 1) % count : ((idx == 0) ? count - 1 : idx - 1);
    app->baud_option_index = idx;
}

static bool connect_settings_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        app->connect_settings_selected = (app->connect_settings_selected == 0) ?
                                              (uint8_t)(ConnectSettingsRowCount - 1) :
                                              (uint8_t)(app->connect_settings_selected - 1);
        with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->connect_settings_selected =
            (uint8_t)((app->connect_settings_selected + 1) % ConnectSettingsRowCount);
        with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyLeft:
    case InputKeyRight: {
        int delta = (event->key == InputKeyRight) ? 1 : -1;
        if(app->connect_settings_selected == ConnectSettingsRowPins) {
            connect_settings_cycle_pin(app, delta);
            with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        } else if(app->connect_settings_selected == ConnectSettingsRowBaud) {
            connect_settings_cycle_baud(app, delta);
            with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    }
    case InputKeyOk:
        if(app->connect_settings_selected == ConnectSettingsRowStart) {
            app_probe_uart_selected(app);
        }
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* connect_settings_view_alloc(App* app) {
    s_connect_settings_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, connect_settings_draw_cb);
    view_set_input_callback(view, connect_settings_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void connect_settings_view_free(View* view) {
    s_connect_settings_app = NULL;
    view_free(view);
}

void connect_settings_view_reset(App* app) {
    app->connect_settings_selected = ConnectSettingsRowPins;
}
