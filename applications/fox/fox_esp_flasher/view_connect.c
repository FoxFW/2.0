#include "fox_esp_flasher.h"
#include <stdio.h>

static FlasherApp* s_app = NULL;

typedef enum {
    ConnectRowPins = 0,
    ConnectRowRetry,
    ConnectRowCount,
} ConnectRow;

#define BOX_X 4
#define BOX_W 120
#define BOX_H 22
#define BOX_R 4

static const uint8_t k_box_y[ConnectRowCount] = {13, 38};

static void connect_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    FlasherApp* app = s_app;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Connect Settings");

    for(size_t row = 0; row < ConnectRowCount; row++) {
        bool selected = (row == (size_t)app->connect_selected);
        uint8_t y     = k_box_y[row];

        char text[40];
        if(row == ConnectRowPins) {
            snprintf(text, sizeof(text), "Pins: %s",
                     flasher_pin_option_label(app->pin_option_index));
        } else {
            snprintf(text, sizeof(text), "Retry");
        }

        canvas_set_color(canvas, ColorBlack);
        if(selected) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_draw_rframe(canvas, BOX_X + 2, y + 2, BOX_W - 4, BOX_H - 4, BOX_R - 2);
        }

        uint8_t ty = y + BOX_H / 2;
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, ty, AlignCenter, AlignCenter, text);

        if(row == ConnectRowPins) {
            canvas_draw_str_aligned(canvas, BOX_X + 6,          ty, AlignLeft,  AlignCenter, "<");
            canvas_draw_str_aligned(canvas, BOX_X + BOX_W - 6,  ty, AlignRight, AlignCenter, ">");
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool connect_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        app->connect_selected = (app->connect_selected == 0)
                                    ? (uint8_t)(ConnectRowCount - 1)
                                    : (uint8_t)(app->connect_selected - 1);
        with_view_model(app->connect_view, uint8_t* _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->connect_selected = (uint8_t)((app->connect_selected + 1) % ConnectRowCount);
        with_view_model(app->connect_view, uint8_t* _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyLeft:
    case InputKeyRight: {
        if(app->connect_selected == ConnectRowPins) {
            size_t cnt = flasher_pin_option_count();
            if(cnt > 1) {
                if(event->key == InputKeyRight) {
                    app->pin_option_index = (app->pin_option_index + 1) % cnt;
                } else {
                    app->pin_option_index = (app->pin_option_index == 0)
                                               ? cnt - 1
                                               : app->pin_option_index - 1;
                }
            }
            with_view_model(app->connect_view, uint8_t* _m, { UNUSED(_m); }, true);
        }
        return true;
    }
    case InputKeyOk:
        if(app->connect_selected == ConnectRowRetry) {
            view_detect_set_probing(app->detect_view, true);
            app->current_view = FlasherViewDetect;
            view_dispatcher_switch_to_view(app->view_dispatcher, FlasherViewDetect);
            view_dispatcher_send_custom_event(app->view_dispatcher, 99);
        }
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_connect_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, connect_draw);
    view_set_input_callback(v, connect_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void view_connect_free(View* v) {
    s_app = NULL;
    view_free(v);
}
