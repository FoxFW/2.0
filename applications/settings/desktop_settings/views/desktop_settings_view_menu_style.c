#include "desktop_settings_view_menu_style.h"
#include <gui/elements.h>
#include <gui/icon.h>
#include "desktop_settings_icons.h"
#include <furi.h>

#define ROW_COUNT     5
#define ROWS_VISIBLE  3
#define BOX_X         4
#define BOX_W         120
#define BOX_H         18
#define BOX_R         4
#define ICON_GAP      3
#define TEXT_PAD      3

static const uint8_t k_slot_y[ROWS_VISIBLE] = {2, 22, 42};

static const char* const k_row_label[ROW_COUNT] =
    {"Fox Theme", "Carousel", "Slider", "Tiny", "Classic"};
static const uint8_t k_row_style_index[ROW_COUNT] = {1, 2, 3, 4, 0};

typedef struct {
    uint8_t cursor;
    uint8_t active;
} DesktopSettingsMenuStyleModel;

struct DesktopSettingsViewMenuStyle {
    View* view;
    DesktopSettingsViewMenuStyleCallback callback;
    void* context;
};

static uint8_t menu_style_slot_for_index(uint8_t style_index) {
    for(uint8_t i = 0; i < ROW_COUNT; i++) {
        if(k_row_style_index[i] == style_index) return i;
    }
    return 0;
}

static uint8_t menu_style_scroll_top(uint8_t cursor) {
    uint8_t max_top = ROW_COUNT - ROWS_VISIBLE;
    return (cursor > max_top) ? max_top : cursor;
}

static void menu_style_draw_cb(Canvas* canvas, void* model_ptr) {
    DesktopSettingsMenuStyleModel* m = model_ptr;
    canvas_clear(canvas);

    uint8_t top = menu_style_scroll_top(m->cursor);

    for(uint8_t slot = 0; slot < ROWS_VISIBLE; slot++) {
        uint8_t idx = top + slot;
        if(idx >= ROW_COUNT) break;
        bool at_cursor = (idx == m->cursor);
        bool is_active = (idx == m->active);
        uint8_t y = k_slot_y[slot];

        canvas_set_color(canvas, ColorBlack);
        if(at_cursor) {
            canvas_draw_rbox(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, BOX_X, y, BOX_W, BOX_H, BOX_R);
        }

        uint8_t icon_x = BOX_X + TEXT_PAD;
        uint8_t icon_y = y + (BOX_H - icon_get_height(&I_ButtonCenter_7x7)) / 2;
        uint8_t text_x = icon_x + icon_get_width(&I_ButtonCenter_7x7) + ICON_GAP;

        if(is_active) {
            canvas_draw_icon(canvas, icon_x, icon_y, &I_ButtonCenter_7x7);
        } else {
            canvas_draw_circle(
                canvas,
                icon_x + icon_get_width(&I_ButtonCenter_7x7) / 2,
                icon_y + icon_get_height(&I_ButtonCenter_7x7) / 2,
                icon_get_width(&I_ButtonCenter_7x7) / 2);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, text_x, y + BOX_H / 2, AlignLeft, AlignCenter, k_row_label[idx]);

        canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar(canvas, m->cursor, ROW_COUNT);
}

static bool menu_style_input_cb(InputEvent* event, void* context) {
    DesktopSettingsViewMenuStyle* instance = context;
    if(event->type != InputTypeShort) return false;

    bool consumed = false;
    bool activated = false;
    uint8_t new_style_index = 0;

    with_view_model(
        instance->view,
        DesktopSettingsMenuStyleModel* m,
        {
            if(event->key == InputKeyUp) {
                m->cursor = (m->cursor == 0) ? (uint8_t)(ROW_COUNT - 1) : m->cursor - 1;
                consumed = true;
            } else if(event->key == InputKeyDown) {
                m->cursor = (uint8_t)((m->cursor + 1) % ROW_COUNT);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                if(m->active != m->cursor) {
                    m->active = m->cursor;
                    activated = true;
                }
                new_style_index = k_row_style_index[m->active];
                consumed = true;
            }
        },
        consumed);

    if(activated && instance->callback) {
        instance->callback(instance->context, new_style_index);
    }

    return consumed;
}

DesktopSettingsViewMenuStyle* desktop_settings_view_menu_style_alloc(void) {
    DesktopSettingsViewMenuStyle* instance = malloc(sizeof(DesktopSettingsViewMenuStyle));
    instance->view     = view_alloc();
    instance->callback = NULL;
    instance->context  = NULL;
    view_set_context(instance->view, instance);
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(DesktopSettingsMenuStyleModel));
    view_set_draw_callback(instance->view, menu_style_draw_cb);
    view_set_input_callback(instance->view, menu_style_input_cb);

    with_view_model(
        instance->view,
        DesktopSettingsMenuStyleModel* m,
        {
            m->cursor = 0;
            m->active = 0;
        },
        false);

    return instance;
}

void desktop_settings_view_menu_style_free(DesktopSettingsViewMenuStyle* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* desktop_settings_view_menu_style_get_view(DesktopSettingsViewMenuStyle* instance) {
    furi_assert(instance);
    return instance->view;
}

void desktop_settings_view_menu_style_set_callback(
    DesktopSettingsViewMenuStyle* instance,
    DesktopSettingsViewMenuStyleCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context  = context;
}

void desktop_settings_view_menu_style_set_selected(
    DesktopSettingsViewMenuStyle* instance,
    uint8_t style_index) {
    furi_assert(instance);
    uint8_t slot = menu_style_slot_for_index(style_index);
    with_view_model(
        instance->view,
        DesktopSettingsMenuStyleModel* m,
        {
            m->cursor = slot;
            m->active = slot;
        },
        false);
}
