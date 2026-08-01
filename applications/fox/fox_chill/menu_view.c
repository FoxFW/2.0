#include "menu_view.h"
#include "fox_chill_events.h"

#include <string.h>

typedef struct {
    uint8_t selected;
    uint8_t offset;
} MenuModel;

#define MENU_ITEM_COUNT 9
#define MENU_BOX_X 2
#define MENU_BOX_W 114
#define MENU_BOX_H 28
#define MENU_BOX_R 4
#define MENU_TEXT_CX (MENU_BOX_X + MENU_BOX_W / 2)

static const uint8_t k_slot_y[2] = {2, 34};
#define MENU_SCROLL_Y0 (k_slot_y[0])
#define MENU_SCROLL_H (k_slot_y[1] + MENU_BOX_H - k_slot_y[0])
#define MENU_SCROLL_X (MENU_BOX_X + MENU_BOX_W + 4)

static const char* k_line1[MENU_ITEM_COUNT] = {
    "Tell me a Joke",
    "Tell me a Riddle",
    "Show a Fun Fact",
    "Random Statistics",
    "Your Mother",
    "Scroll Counter",
    "Be Mindful",
    "A Long Wait",
    "Usage",
};
static const char* k_line2[MENU_ITEM_COUNT] = {
    "One random one-liner",
    "Can you solve it?",
    "Something you didn't know",
    "Statistic based facts",
    "Classic yo mama jokes",
    "How high can you go?",
    "Mindfulness reminder",
    "Test your patience",
    "Your Fox Chill progress",
};

static void menu_draw_cb(Canvas* canvas, void* model_ptr) {
    MenuModel* m = model_ptr;
    canvas_clear(canvas);

    for(uint8_t slot = 0; slot < 2; slot++) {
        uint8_t idx = m->offset + slot;
        if(idx >= MENU_ITEM_COUNT) break;

        bool sel = (idx == m->selected);
        uint8_t y = k_slot_y[slot];

        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, MENU_BOX_X, y, MENU_BOX_W, MENU_BOX_H, MENU_BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, MENU_BOX_X, y, MENU_BOX_W, MENU_BOX_H, MENU_BOX_R);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, MENU_TEXT_CX, y + 9, AlignCenter, AlignCenter, k_line1[idx]);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, MENU_TEXT_CX, y + 20, AlignCenter, AlignCenter, k_line2[idx]);
        canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar_pos(
        canvas, MENU_SCROLL_X, MENU_SCROLL_Y0, MENU_SCROLL_H, m->selected, MENU_ITEM_COUNT);
}

static bool menu_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(app->menu_view, MenuModel* m, {
            if(m->selected > 0) {
                m->selected--;
                if(m->selected < m->offset) m->offset = m->selected;
            }
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->menu_view, MenuModel* m, {
            if(m->selected < MENU_ITEM_COUNT - 1) {
                m->selected++;
                if(m->selected > m->offset + 1) m->offset = m->selected - 1;
            }
        }, true);
        return true;
    case InputKeyOk:
    case InputKeyRight: {
        uint8_t sel = 0;
        with_view_model(app->menu_view, MenuModel* m, { sel = m->selected; }, false);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FoxChillEventMenuSelectBase + sel);
        return true;
    }
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* menu_view_alloc(App* app) {
    View* v = view_alloc();
    view_set_draw_callback(v, menu_draw_cb);
    view_set_input_callback(v, menu_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(MenuModel));
    with_view_model(v, MenuModel* m, { m->selected = 0; m->offset = 0; }, false);
    return v;
}

void menu_view_free(View* view) {
    view_free(view);
}
