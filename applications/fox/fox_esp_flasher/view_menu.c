#include "fox_esp_flasher.h"

typedef struct {
    uint8_t selected; /* 0-2 */
    uint8_t offset;   /* 0 = show items 0,1  |  1 = show items 1,2 */
} MenuModel;

#define ITEM_COUNT 3
#define BOX_X  4
#define BOX_W  120
#define BOX_H  28
#define BOX_R  4

static const uint8_t k_slot_y[2] = {2, 34};  /* 2 + 28 + 4gap + 28 = 62, fits in 64px */

static const char* k_line1[ITEM_COUNT] = {
    "Install",
    "Custom Install",
    "Terminal",
};
static const char* k_line2[ITEM_COUNT] = {
    "Fox ESP32 Firmware",
    "Select .bin files manually",
    "Post command to Terminal",
};

static void menu_draw(Canvas* canvas, void* model_ptr) {
    MenuModel* m = model_ptr;
    canvas_clear(canvas);

    for(uint8_t slot = 0; slot < 2; slot++) {
        uint8_t idx = m->offset + slot;
        if(idx >= ITEM_COUNT) break;

        bool sel = (idx == m->selected);
        uint8_t y = k_slot_y[slot];

        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, y + 9, AlignCenter, AlignCenter, k_line1[idx]);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, y + 20, AlignCenter, AlignCenter, k_line2[idx]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool menu_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(app->menu_view, MenuModel* m, {
            if(m->selected > 0) {
                m->selected--;
                if(m->selected < m->offset) m->offset--;
            }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->menu_view, MenuModel* m, {
            if(m->selected < ITEM_COUNT - 1) {
                m->selected++;
                if(m->selected > m->offset + 1) m->offset++;
            }
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
    View* v = view_alloc();
    view_set_draw_callback(v, menu_draw);
    view_set_input_callback(v, menu_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(MenuModel));
    with_view_model(v, MenuModel* m, { m->selected = 0; m->offset = 0; }, false);
    return v;
}

void view_menu_free(View* v) {
    view_free(v);
}
