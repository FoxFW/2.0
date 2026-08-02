#include "app.h"

typedef struct {
    uint8_t selected;
} MenuModel;

#define BOX_X 4
#define BOX_W 120
#define BOX_H 28
#define BOX_R 4
#define MENU_ROW_COUNT 3
#define MENU_VISIBLE 2

static const uint8_t k_slot_y[MENU_VISIBLE] = {2, 34};
static const char* const k_line1[MENU_ROW_COUNT] = {
    "Fox Custom Firmware", "Fox ESP32 Firmware", "Download Settings"};
static const char* const k_line2[MENU_ROW_COUNT] = {
    "Flipper Zero", "ESP32 S2, S3, C3, C5, C6", "Edit Baud & Retry Options"};

static uint8_t menu_scroll_top(uint8_t selected) {
    return (selected >= MENU_ROW_COUNT - 1) ? (uint8_t)(MENU_ROW_COUNT - MENU_VISIBLE) : 0;
}

static void menu_draw(Canvas* canvas, void* model_ptr) {
    MenuModel* m = model_ptr;
    canvas_clear(canvas);
    uint8_t scroll_top = menu_scroll_top(m->selected);
    for(uint8_t slot = 0; slot < MENU_VISIBLE; slot++) {
        uint8_t row = scroll_top + slot;
        bool sel = (m->selected == row);
        uint8_t y = k_slot_y[slot];
        canvas_set_color(canvas, ColorBlack);
        if(sel) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, y + 9, AlignCenter, AlignCenter, k_line1[row]);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, y + 20, AlignCenter, AlignCenter, k_line2[row]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool menu_input(InputEvent* event, void* context) {
    UpdaterApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:
        with_view_model(
            app->menu_view,
            MenuModel * m,
            { m->selected = (m->selected == 0) ? (MENU_ROW_COUNT - 1) : m->selected - 1; },
            true);
        return true;
    case InputKeyDown:
        with_view_model(
            app->menu_view,
            MenuModel * m,
            { m->selected = (uint8_t)((m->selected + 1) % MENU_ROW_COUNT); },
            true);
        return true;
    case InputKeyOk: {
        uint8_t sel = 0;
        with_view_model(app->menu_view, MenuModel * m, { sel = m->selected; }, false);
        UpdaterEvent ev = UpdaterEventMenuFw;
        if(sel == 1) ev = UpdaterEventMenuEsp32;
        else if(sel == 2) ev = UpdaterEventMenuDownloadSettings;
        view_dispatcher_send_custom_event(app->view_dispatcher, ev);
        return true;
    }
    default:
        return false;
    }
}

View* view_menu_alloc(UpdaterApp* app) {
    View* v = view_alloc();
    view_set_draw_callback(v, menu_draw);
    view_set_input_callback(v, menu_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(MenuModel));
    with_view_model(v, MenuModel * m, { m->selected = 0; }, false);
    return v;
}

void view_menu_free(View* v) {
    view_free(v);
}
