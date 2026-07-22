/* view_result.c — Success / Error result screen shown after flashing. */

#include "fox_esp_flasher.h"

static FlasherApp* s_app = NULL;

typedef struct {
    bool    success;
    uint8_t board_index;
} ResultModel;

#define BTN_X 4
#define BTN_W 120
#define BTN_H 16
#define BTN_R 4

static void result_draw(Canvas* canvas, void* model_ptr) {
    ResultModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->success) {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Flash Complete!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Fox ESP32 Firmware");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "installed successfully.");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "ESP32 is restarting...");
    } else {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Flash Failed");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Check wiring and");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "board selection,");
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "then try again.");
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, BTN_X, 50, BTN_W, BTN_H, BTN_R);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 50 + BTN_H / 2, AlignCenter, AlignCenter, "Back to Menu");
    canvas_set_color(canvas, ColorBlack);
}

static bool result_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk || event->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FlasherViewMenu);
        return true;
    }
    return false;
}

View* view_result_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, result_draw);
    view_set_input_callback(v, result_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(ResultModel));
    with_view_model(v, ResultModel* m, {
        m->success     = false;
        m->board_index = 0;
    }, false);
    return v;
}

void view_result_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_result_set(View* v, bool success, uint8_t board_index) {
    with_view_model(v, ResultModel* m, {
        m->success     = success;
        m->board_index = board_index;
    }, true);
}
