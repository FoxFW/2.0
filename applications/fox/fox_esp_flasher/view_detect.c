#include "fox_esp_flasher.h"
#include <string.h>

static FlasherApp* s_app = NULL;

typedef struct {
    bool probing;
    bool found;
    uint8_t dots;       /* 0-3, animated while probing */
    uint8_t selected;   /* 0 = Settings, 1 = Retry (not-found mode) */
} DetectModel;

#define BOX_X 4
#define BOX_W 55
#define BOX_H 20
#define BOX_R 4
static const uint8_t k_box_x[2] = {4, 66};

static void detect_draw(Canvas* canvas, void* model_ptr) {
    DetectModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->probing) {
        static const char* dots_str[] = {"", ".", "..", "..."};
        char buf[32];
        snprintf(buf, sizeof(buf), "Detecting ESP32%s", dots_str[m->dots % 4]);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, buf);
        return;
    }

    if(m->found) {
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "ESP32 detected!");
        return;
    }

    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Fox ESP Flasher");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 17, AlignCenter, AlignTop, "ESP32 not detected.");
    canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignTop, "Connect ESP32 via UART,");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, "then press Retry.");

    const char* labels[2] = {"Settings", "Retry"};
    for(uint8_t i = 0; i < 2; i++) {
        uint8_t bx = k_box_x[i];
        uint8_t by = 44;
        canvas_set_color(canvas, ColorBlack);
        if(m->selected == i) {
            canvas_draw_rbox(canvas, bx, by, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, bx, by, BOX_W, BOX_H, BOX_R);
            canvas_draw_rframe(canvas, bx + 2, by + 2, BOX_W - 4, BOX_H - 4, BOX_R - 2);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, bx + BOX_W / 2, by + BOX_H / 2,
                                AlignCenter, AlignCenter, labels[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool detect_input(InputEvent* event, void* context) {
    FlasherApp* app = context;

    bool busy = false;
    with_view_model(app->detect_view, DetectModel* m, {
        busy = m->probing || m->found;
    }, false);
    if(busy) return false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight: {
        with_view_model(
            app->detect_view,
            DetectModel* m,
            { m->selected ^= 1; },
            true);
        return true;
    }
    case InputKeyOk: {
        uint8_t sel = 0;
        with_view_model(app->detect_view, DetectModel* m, { sel = m->selected; }, false);
        if(sel == 0) {
            app->current_view = FlasherViewConnect;
            view_dispatcher_switch_to_view(app->view_dispatcher, FlasherViewConnect);
        } else {
            view_detect_set_probing(app->detect_view, true);
            view_dispatcher_send_custom_event(app->view_dispatcher, 99);
        }
        return true;
    }
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_detect_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, detect_draw);
    view_set_input_callback(v, detect_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(DetectModel));
    with_view_model(v, DetectModel* m, {
        m->probing  = true;
        m->found    = false;
        m->dots     = 0;
        m->selected = 1; /* default to Retry */
    }, false);
    return v;
}

void view_detect_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_detect_set_probing(View* v, bool probing) {
    with_view_model(v, DetectModel* m, { m->probing = probing; }, true);
}

void view_detect_set_found(View* v, bool found) {
    with_view_model(v, DetectModel* m, {
        m->probing = false;
        m->found   = found;
    }, true);
}
