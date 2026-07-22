#include "fox_esp_flasher.h"

#define BTN_X  4
#define BTN_W  120
#define BTN_H  14
#define BTN_R  4
#define BTN_Y  49

static void prepare_draw(Canvas* canvas, void* model_ptr) {
    UNUSED(model_ptr);
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Prepare ESP32");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, "Hold BOOT on ESP32,");
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "tap RST to restart,");
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, "release BOOT, then:");

    canvas_draw_rbox(canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, BTN_Y + BTN_H / 2, AlignCenter, AlignCenter, "Flash!");
    canvas_set_color(canvas, ColorBlack);
}

static bool prepare_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventPrepareGo);
        return true;
    }
    return false;
}

View* view_prepare_alloc(FlasherApp* app) {
    View* v = view_alloc();
    view_set_draw_callback(v, prepare_draw);
    view_set_input_callback(v, prepare_input);
    view_set_context(v, app);
    return v;
}

void view_prepare_free(View* v) {
    view_free(v);
}
