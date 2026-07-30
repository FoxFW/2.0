#include "app.h"

static UpdaterApp* s_app = NULL;

#define MESSAGE_BOTTOM_BAR_H 16

static void message_draw_two_buttons(
    Canvas* canvas, bool focus_left, const char* left_label, const char* right_label) {
    int32_t bar_y = 64 - MESSAGE_BOTTOM_BAR_H;
    int32_t btn_gap = 4;
    int32_t btn_w = (128 - btn_gap * 3) / 2;
    int32_t left_x = btn_gap;
    int32_t right_x = btn_gap * 2 + btn_w;

    canvas_set_color(canvas, ColorBlack);
    if(focus_left) {
        canvas_draw_rbox(canvas, left_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, left_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, left_label);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, right_label);
    } else {
        canvas_draw_rframe(canvas, left_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_draw_str_aligned(
            canvas, left_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, left_label);
        canvas_draw_rbox(canvas, right_x, bar_y, btn_w, MESSAGE_BOTTOM_BAR_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + MESSAGE_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, right_label);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void message_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    UpdaterApp* app = s_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->message_view_detecting) {
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Detecting ESP32...");
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Checking UART pins");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "Fox ESP32 Firmware");
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "required on ESP32");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "connected via GPIO.");

    message_draw_two_buttons(
        canvas, app->message_view_not_detected_focus_left, "Settings", "Retry");
}

static bool message_input_cb(InputEvent* event, void* context) {
    UpdaterApp* app = context;
    if(event->type != InputTypeShort) return false;

    if(app->message_view_detecting) return false;

    switch(event->key) {
    case InputKeyLeft:
        if(!app->message_view_not_detected_focus_left) {
            app->message_view_not_detected_focus_left = true;
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->message_view_not_detected_focus_left) {
            app->message_view_not_detected_focus_left = false;
            with_view_model(app->message_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->message_view_not_detected_focus_left) {
            connect_settings_view_reset(app);
            app->current_view = UpdaterViewConnectSettings;
            view_dispatcher_switch_to_view(app->view_dispatcher, UpdaterViewConnectSettings);
            return true;
        }
        updater_retry_detection(app);
        return true;
    default:
        return false;
    }
}

View* view_message_alloc(UpdaterApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, message_draw_cb);
    view_set_input_callback(v, message_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void view_message_free(View* v) {
    s_app = NULL;
    view_free(v);
}
