#include "restart_confirm_view.h"

#include <furi_hal_power.h>

static App* s_restart_confirm_app = NULL;

#define RESTART_CONFIRM_BUTTON_H 14
#define RESTART_CONFIRM_BUTTON_PAD_X 4
#define RESTART_CONFIRM_BUTTON_MARGIN 2
#define RESTART_CONFIRM_BUTTON_R 3

static void restart_confirm_draw_button(
    Canvas* canvas, bool align_left, bool focused, const char* text) {
    canvas_set_font(canvas, FontSecondary);
    uint16_t text_w = canvas_string_width(canvas, text);
    uint16_t box_w = text_w + RESTART_CONFIRM_BUTTON_PAD_X * 2;
    uint16_t box_h = RESTART_CONFIRM_BUTTON_H;
    int32_t box_y = 64 - RESTART_CONFIRM_BUTTON_MARGIN - box_h;
    int32_t box_x = align_left ? RESTART_CONFIRM_BUTTON_MARGIN :
                                  (128 - RESTART_CONFIRM_BUTTON_MARGIN - (int32_t)box_w);

    canvas_set_color(canvas, ColorBlack);
    if(focused) {
        canvas_draw_rbox(canvas, box_x, box_y, box_w, box_h, RESTART_CONFIRM_BUTTON_R);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, box_x + box_w / 2, box_y + box_h / 2, AlignCenter, AlignCenter, text);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, box_x, box_y, box_w, box_h, RESTART_CONFIRM_BUTTON_R);
        canvas_draw_str_aligned(
            canvas, box_x + box_w / 2, box_y + box_h / 2, AlignCenter, AlignCenter, text);
    }
}

static void restart_confirm_draw_bottom_bar(Canvas* canvas, bool focus_left) {
    restart_confirm_draw_button(canvas, true, focus_left, "< Later");
    restart_confirm_draw_button(canvas, false, !focus_left, "Restart >");
}

static void restart_confirm_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_restart_confirm_app;
    if(app == NULL) return;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignCenter, "Restart now to apply");
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Device Name Update?");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "Later applies it next");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "time you boot instead.");

    restart_confirm_draw_bottom_bar(canvas, app->restart_confirm_focus_left);
}

static bool restart_confirm_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyBack:

        app->device_name_restart_pending = false;
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    case InputKeyLeft:
        if(!app->restart_confirm_focus_left) {
            app->restart_confirm_focus_left = true;
            with_view_model(app->restart_confirm_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
        if(app->restart_confirm_focus_left) {
            app->restart_confirm_focus_left = false;
            with_view_model(app->restart_confirm_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
        if(app->restart_confirm_focus_left) {

            app->device_name_restart_pending = false;
            view_dispatcher_stop(app->view_dispatcher);
        } else {
            app->device_name_restart_pending = false;
            furi_hal_power_reset();
        }
        return true;
    default:
        return false;
    }
}

View* restart_confirm_view_alloc(App* app) {
    s_restart_confirm_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, restart_confirm_draw_cb);
    view_set_input_callback(view, restart_confirm_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void restart_confirm_view_free(View* view) {
    s_restart_confirm_app = NULL;
    view_free(view);
}
