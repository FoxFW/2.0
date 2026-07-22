#include "fox_esp_flasher.h"

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t selected; /* 0, 1, 2 */
} MenuModel;

#define BOX_X 4
#define BOX_W 120
#define BOX_H 18
#define BOX_R 4

static const uint8_t k_box_y[3] = {11, 31, 51};

static const char* k_line1[3] = {
    "Install Fox ESP32 Firmware",
    "Install Custom .bin Files",
    "Terminal",
};
static const char* k_line2[3] = {
    NULL,
    "(Select files manually)",
    "Send commands & view output",
};

static void menu_draw(Canvas* canvas, void* model_ptr) {
    MenuModel* m = model_ptr;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Fox ESP Flasher");

    for(uint8_t i = 0; i < 3; i++) {
        bool sel = (m->selected == i);
        uint8_t y = k_box_y[i];

        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_draw_rframe(canvas, BOX_X + 2, y + 2, BOX_W - 4, BOX_H - 4, BOX_R - 2);
        }

        canvas_set_font(canvas, FontSecondary);
        uint8_t text_y = y + BOX_H / 2;
        if(k_line2[i]) {
            canvas_draw_str_aligned(canvas, 64, text_y - 3, AlignCenter, AlignCenter, k_line1[i]);
            canvas_draw_str_aligned(canvas, 64, text_y + 5, AlignCenter, AlignCenter, k_line2[i]);
        } else {
            canvas_draw_str_aligned(canvas, 64, text_y, AlignCenter, AlignCenter, k_line1[i]);
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool menu_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(app->menu_view, MenuModel* m, {
            m->selected = (m->selected == 0) ? 2 : m->selected - 1;
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->menu_view, MenuModel* m, {
            m->selected = (m->selected + 1) % 3;
        }, true);
        return true;
    case InputKeyOk: {
        uint8_t sel = 0;
        with_view_model(app->menu_view, MenuModel* m, { sel = m->selected; }, false);
        FlasherEvent ev = (sel == 0) ? FlasherEventMenuFirmware
                        : (sel == 1) ? FlasherEventMenuCustom
                                     : FlasherEventMenuTerminal;
        view_dispatcher_send_custom_event(app->view_dispatcher, ev);
        return true;
    }
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_menu_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, menu_draw);
    view_set_input_callback(v, menu_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(MenuModel));
    with_view_model(v, MenuModel* m, { m->selected = 0; }, false);
    return v;
}

void view_menu_free(View* v) {
    s_app = NULL;
    view_free(v);
}
