#include "fox_esp_flasher.h"

static FlasherApp* s_prepare_app = NULL;

typedef struct {
    bool    show_cancel;
    uint8_t selected;
    uint8_t _tick;
} PrepareModel;

#define CONT_X  26
#define CONT_W  76
#define CONT_H  18
#define CONT_R  4
#define CONT_Y  47

#define BTN_W   55
#define BTN_H   18
#define BTN_R   4
static const uint8_t k_btn_x[2] = {4, 66};

static void prepare_draw(Canvas* canvas, void* model_ptr) {
    PrepareModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Enter Boot Mode");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, "Hold BOOT on ESP32,");
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignTop, "tap RST, release BOOT.");

    if(!m->show_cancel) {
        flasher_draw_ok_button(canvas, CONT_X, CONT_Y, CONT_W, CONT_H, CONT_R, "Continue");
    } else {
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "Then press Retry.");
        const char* labels[2] = {"< Cancel", "Retry"};
        for(uint8_t i = 0; i < 2; i++) {
            uint8_t bx = k_btn_x[i];
            uint8_t by = 47;
            if(m->selected == i) {
                flasher_draw_ok_button(canvas, bx, by, BTN_W, BTN_H, BTN_R, labels[i]);
            } else {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, bx, by, BTN_W, BTN_H, BTN_R);
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(
                    canvas, bx + BTN_W / 2, by + BTN_H / 2,
                    AlignCenter, AlignCenter, labels[i]);
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }
}

static bool prepare_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool show_cancel = false;
    uint8_t sel = 1;
    with_view_model(app->prepare_view, PrepareModel* m, {
        show_cancel = m->show_cancel;
        sel         = m->selected;
    }, false);

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight:
        if(show_cancel) {
            with_view_model(
                app->prepare_view, PrepareModel* m, { m->selected ^= 1; }, true);
        }
        return true;
    case InputKeyOk:
        if(!show_cancel) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, FlasherEventPrepareContinue);
        } else {
            FlasherEvent ev = (sel == 0) ? FlasherEventPrepareCancel : FlasherEventPrepareGo;
            view_dispatcher_send_custom_event(app->view_dispatcher, ev);
        }
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_prepare_alloc(FlasherApp* app) {
    s_prepare_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, prepare_draw);
    view_set_input_callback(v, prepare_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(PrepareModel));
    with_view_model(v, PrepareModel* m, {
        m->show_cancel = false;
        m->selected    = 1;
        m->_tick       = 0;
    }, false);
    return v;
}

void view_prepare_free(View* v) {
    s_prepare_app = NULL;
    view_free(v);
}

void view_prepare_refresh(View* v) {
    with_view_model(v, PrepareModel* m, { m->_tick++; }, true);
}

void view_prepare_set_startup(View* v) {
    with_view_model(v, PrepareModel* m, {
        m->show_cancel = false;
        m->selected    = 1;
    }, true);
}

void view_prepare_set_error(View* v) {
    with_view_model(v, PrepareModel* m, {
        m->show_cancel = true;
        m->selected    = 1;
    }, true);
}
