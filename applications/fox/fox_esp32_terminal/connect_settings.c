#include "connect_settings.h"
#include "fox_esp32_terminal_icons.h"
#include <gui/icon.h>
#include "gpio_remap_compat.h"

#include <stdio.h>

static App* s_connect_settings_app = NULL;

typedef enum {
    ConnectSettingsRowPins,
    ConnectSettingsRowBaud,
    ConnectSettingsRowStart,
    ConnectSettingsRowCount,
} ConnectSettingsRow;

#define CONNECT_ROW_X   4
#define CONNECT_ROW_W   120
#define CONNECT_ROW_R   4
#define CONNECT_ROW_TOP 14
#define CONNECT_ROW_H   16
#define CONNECT_ROW_GAP 3
#define CONNECT_RETRY_Y 52
#define CONNECT_RETRY_H 11
#define CONNECT_RETRY_R 3

static void connect_settings_draw_option_row(
    Canvas* canvas, int32_t y, int32_t h, bool selected, const char* row_text) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, CONNECT_ROW_X, y, CONNECT_ROW_W, h, CONNECT_ROW_R);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, CONNECT_ROW_X, y, CONNECT_ROW_W, h, CONNECT_ROW_R);
    }

    int32_t text_y = y + h / 2;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, row_text);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, CONNECT_ROW_X + 8, text_y, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(
        canvas, CONNECT_ROW_X + CONNECT_ROW_W - 8, text_y, AlignRight, AlignCenter, ">");

    canvas_set_color(canvas, ColorBlack);
}

static void connect_settings_draw_action_button(
    Canvas* canvas, bool selected, int32_t y, int32_t h, int32_t radius, const char* label) {
    canvas_set_font(canvas, FontSecondary);
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_w = icon_get_width(icon);
    int32_t icon_h = icon_get_height(icon);
    int32_t icon_gap = 3;
    int32_t pad_x = 10;
    int32_t content_w = icon_w + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t box_w = content_w + pad_x * 2;
    int32_t x = (128 - box_w) / 2;

    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, x, y, box_w, h, radius);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, box_w, h, radius);
    }

    int32_t gx = x + (box_w - content_w) / 2;
    int32_t gy_icon = y + (h - icon_h) / 2;
    canvas_draw_icon(canvas, gx, gy_icon, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon_w + icon_gap, y + h / 2, AlignLeft, AlignCenter, label);

    canvas_set_color(canvas, ColorBlack);
}

static void connect_settings_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_connect_settings_app;
    if(app == NULL || canvas == NULL) return;

    if(app->connect_settings_selected >= ConnectSettingsRowCount) {
        app->connect_settings_selected = 0;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Connect Settings");

    int32_t y0 = CONNECT_ROW_TOP;
    int32_t y1 = y0 + CONNECT_ROW_H + CONNECT_ROW_GAP;

    char pins_text[32];
    snprintf(pins_text, sizeof(pins_text), "Pins: %s", app_pin_option_label(app->pin_option_index));
    connect_settings_draw_option_row(
        canvas, y0, CONNECT_ROW_H, app->connect_settings_selected == ConnectSettingsRowPins, pins_text);

    char baud_text[32];
    snprintf(
        baud_text,
        sizeof(baud_text),
        "Baud: %lu",
        (unsigned long)app_baud_option_value(app->baud_option_index));
    connect_settings_draw_option_row(
        canvas, y1, CONNECT_ROW_H, app->connect_settings_selected == ConnectSettingsRowBaud, baud_text);

    connect_settings_draw_action_button(
        canvas,
        app->connect_settings_selected == ConnectSettingsRowStart,
        CONNECT_RETRY_Y,
        CONNECT_RETRY_H,
        CONNECT_RETRY_R,
        "Retry");
}

static void connect_settings_cycle_pin(App* app, int delta) {
    if(app == NULL) return;
    size_t count = app_pin_option_count();
    if(count <= 1) return;
    size_t idx = app->pin_option_index;
    idx = (delta > 0) ? (idx + 1) % count : ((idx == 0) ? count - 1 : idx - 1);
    app->pin_option_index = idx;

    GpioRemapSettings gpio_remap = {.esp32_uart_channel = (uint8_t)idx};
    gpio_remap_settings_save(&gpio_remap);
}

static void connect_settings_cycle_baud(App* app, int delta) {
    if(app == NULL) return;
    size_t count = app_baud_option_count();
    if(count <= 1) return;
    size_t idx = app->baud_option_index;
    idx = (delta > 0) ? (idx + 1) % count : ((idx == 0) ? count - 1 : idx - 1);
    app->baud_option_index = idx;
}

static bool connect_settings_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(app == NULL || event == NULL) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->connect_settings_selected >= ConnectSettingsRowCount) {
        app->connect_settings_selected = 0;
    }

    switch(event->key) {
    case InputKeyUp:
        app->connect_settings_selected = (app->connect_settings_selected == 0) ?
                                              (uint8_t)(ConnectSettingsRowCount - 1) :
                                              (uint8_t)(app->connect_settings_selected - 1);
        if(app->connect_settings_view != NULL) {
            with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyDown:
        app->connect_settings_selected =
            (uint8_t)((app->connect_settings_selected + 1) % ConnectSettingsRowCount);
        if(app->connect_settings_view != NULL) {
            with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyLeft:
    case InputKeyRight: {
        int delta = (event->key == InputKeyRight) ? 1 : -1;
        if(app->connect_settings_selected == ConnectSettingsRowPins) {
            connect_settings_cycle_pin(app, delta);
            if(app->connect_settings_view != NULL) {
                with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
        } else if(app->connect_settings_selected == ConnectSettingsRowBaud) {
            connect_settings_cycle_baud(app, delta);
            if(app->connect_settings_view != NULL) {
                with_view_model(app->connect_settings_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
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
    if(view == NULL) return NULL;
    view_set_draw_callback(view, connect_settings_draw_cb);
    view_set_input_callback(view, connect_settings_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void connect_settings_view_free(View* view) {
    s_connect_settings_app = NULL;
    if(view != NULL) view_free(view);
}

void connect_settings_view_reset(App* app) {
    if(app == NULL) return;
    app->connect_settings_selected = ConnectSettingsRowPins;

    GpioRemapSettings gpio_remap;
    gpio_remap_settings_load(&gpio_remap);
    size_t count = app_pin_option_count();
    if(gpio_remap.esp32_uart_channel < count) {
        app->pin_option_index = gpio_remap.esp32_uart_channel;
    }
}
