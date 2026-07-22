#include "menu.h"

#include <gui/elements.h>
#include <gui/icon_animation_i.h>
#include <assets_icons.h>
#include <furi.h>
#include <furi_hal.h>
#include <m-array.h>
#include "../../desktop/desktop_settings.h"
#include "fox_theme.h"

struct Menu {
    View* view;
};

typedef struct {
    const char* label;
    IconAnimation* icon;
    uint32_t index;
    MenuItemCallback callback;
    void* callback_context;
} MenuItem;

ARRAY_DEF(MenuItemArray, MenuItem, M_POD_OPLIST); //-V658
#define M_OPL_MenuItemArray_t() ARRAY_OPLIST(MenuItemArray, M_POD_OPLIST)

typedef struct {
    MenuItemArray_t items;
    size_t position;
    uint8_t cached_theme; // loaded once at alloc, updated via menu_set_theme
} MenuModel;

#define FOX_CELL_W   40
#define FOX_CELL_H   30
#define FOX_CELL_GAP  3
#define FOX_COLS      3
#define FOX_ROWS      2
#define FOX_VISIBLE  (FOX_COLS * FOX_ROWS)   // 6 cells shown at once

static void menu_process_up(Menu* menu);
static void menu_process_down(Menu* menu);
static void menu_process_left(Menu* menu);
static void menu_process_right(Menu* menu);
static void menu_process_ok(Menu* menu);

static size_t fox_shift(size_t position, size_t count) {
    if(count <= FOX_VISIBLE) return 0;
    size_t col = position / FOX_ROWS;
    if(col < 1) return 0;
    size_t last_col = (count - 1) / FOX_ROWS;
    if(col + 1 >= last_col) {
        size_t start_col = (last_col + 1 > FOX_COLS) ? last_col + 1 - FOX_COLS : 0;
        return start_col * FOX_ROWS;
    }
    return (col - 1) * FOX_ROWS;
}

static const char* menu_fox_label(const char* label) {
    if(!label) return label;
    if(strcmp(label, "Sub-GHz") == 0)             return "SubGhz";
    if(strcmp(label, "125 kHz RFID") == 0)        return "RFID";
    if(strcmp(label, "Fox Settings") == 0)         return "Fox";
    if(strcmp(label, "Sub-GHz Bruteforcer") == 0) return "S-Brute";
    return label;
}

static void menu_draw_callback(Canvas* canvas, void* _model) {
    MenuModel* model = _model;
    canvas_clear(canvas);

    size_t position    = model->position;
    size_t items_count = MenuItemArray_size(model->items);
    if(!items_count) {
        canvas_draw_str(canvas, 2, 32, "Empty");
        return;
    }

    uint8_t theme = fox_theme_is_active() ? 1u : 0u;
    /* If the theme changed since last draw (e.g. user toggled it in Fox Settings),
     * sync cached_theme so enter/exit animation logic stays consistent. */
    model->cached_theme = theme;

    if(theme == 1) {
            size_t shift = fox_shift(position, items_count);

        for(uint8_t cell = 0; cell < FOX_VISIBLE; cell++) {
            size_t item_idx = shift + cell;
            if(item_idx >= items_count) break;

            uint8_t col = cell / FOX_ROWS;
            uint8_t row = cell % FOX_ROWS;
            int32_t x   = 1 + col * (FOX_CELL_W + FOX_CELL_GAP);
            int32_t y   = row * (FOX_CELL_H + FOX_CELL_GAP);

            MenuItem* item = MenuItemArray_get(model->items, item_idx);
            elements_fox_horizontal_menu_item(
                canvas, x, y, FOX_CELL_W, FOX_CELL_H,
                menu_fox_label(item->label),
                item->icon,            // IconAnimation* — draws current animated frame
                item_idx == position);
        }
    } else {
            MenuItem* item;
        size_t shift_position;

        canvas_set_font(canvas, FontSecondary);
        shift_position = (position + items_count - 1) % items_count;
        item = MenuItemArray_get(model->items, shift_position);
        canvas_draw_icon_animation(canvas, 4, 3, item->icon);
        canvas_draw_str(canvas, 22, 14, item->label);

        canvas_set_font(canvas, FontPrimary);
        shift_position = position;
        item = MenuItemArray_get(model->items, shift_position);
        canvas_draw_icon_animation(canvas, 4, 25, item->icon);
        canvas_draw_str(canvas, 22, 36, item->label);

        canvas_set_font(canvas, FontSecondary);
        shift_position = (position + 1) % items_count;
        item = MenuItemArray_get(model->items, shift_position);
        canvas_draw_icon_animation(canvas, 4, 47, item->icon);
        canvas_draw_str(canvas, 22, 58, item->label);

        elements_frame(canvas, 0, 21, 128 - 5, 21);
        elements_scrollbar(canvas, position, items_count);
    }
}

static bool menu_input_callback(InputEvent* event, void* context) {
    Menu* menu = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            consumed = true;
            menu_process_up(menu);
            break;
        case InputKeyDown:
            consumed = true;
            menu_process_down(menu);
            break;
        case InputKeyLeft:
            consumed = true;
            menu_process_left(menu);
            break;
        case InputKeyRight:
            consumed = true;
            menu_process_right(menu);
            break;
        case InputKeyOk:
            if(event->type != InputTypeRepeat) {
                consumed = true;
                menu_process_ok(menu);
            }
            break;
        default:
            break;
        }
    }
    return consumed;
}

static void menu_enter(void* context) {
    Menu* menu = context;
    with_view_model(menu->view, MenuModel* model, {
        size_t count = MenuItemArray_size(model->items);
        if(count) {
            if(model->cached_theme == 1) {
                // Fox grid: all cells are visible simultaneously — start every animation
                for(size_t i = 0; i < count; i++) {
                    icon_animation_start(MenuItemArray_get(model->items, i)->icon);
                }
            } else {
                // Classic: only the selected item is shown centre-screen
                icon_animation_start(MenuItemArray_get(model->items, model->position)->icon);
            }
        }
    }, false);
}

static void menu_exit(void* context) {
    Menu* menu = context;
    with_view_model(menu->view, MenuModel* model, {
        size_t count = MenuItemArray_size(model->items);
        if(count) {
            if(model->cached_theme == 1) {
                // Fox grid: all were started, stop all
                for(size_t i = 0; i < count; i++) {
                    icon_animation_stop(MenuItemArray_get(model->items, i)->icon);
                }
            } else {
                icon_animation_stop(MenuItemArray_get(model->items, model->position)->icon);
            }
        }
    }, false);
}

Menu* menu_alloc(void) {
    Menu* menu = malloc(sizeof(Menu));
    menu->view = view_alloc();
    view_set_context(menu->view, menu);
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(MenuModel));
    view_set_draw_callback(menu->view, menu_draw_callback);
    view_set_input_callback(menu->view, menu_input_callback);
    view_set_enter_callback(menu->view, menu_enter);
    view_set_exit_callback(menu->view, menu_exit);

    // Load theme before entering with_view_model — furi_record_open inside a locked
    // view model trips Flipper's lock-ordering furi_check. Load here, pass value in.
    uint8_t loaded_theme = MenuThemeFox;
    DesktopSettings* s = malloc(sizeof(DesktopSettings));
    if(s) {
        desktop_settings_load(s);
        loaded_theme = s->menu_theme;
        free(s);
    }

    with_view_model(menu->view, MenuModel* model, {
        MenuItemArray_init(model->items);
        model->position = 0;
        model->cached_theme = loaded_theme;
    }, true);

    return menu;
}

void menu_free(Menu* menu) {
    furi_check(menu);
    menu_reset(menu);
    with_view_model(menu->view, MenuModel* model, {
        MenuItemArray_clear(model->items);
    }, false);
    view_free(menu->view);
    free(menu);
}

View* menu_get_view(Menu* menu) {
    furi_check(menu);
    return menu->view;
}

void menu_add_item(
    Menu* menu,
    const char* label,
    const Icon* icon,
    uint32_t index,
    MenuItemCallback callback,
    void* context) {
    furi_check(menu);
    furi_check(label);
    with_view_model(menu->view, MenuModel* model, {
        MenuItem* item = MenuItemArray_push_new(model->items);
        item->label            = label;
        item->icon             = icon ?
            icon_animation_alloc(icon) : icon_animation_alloc(&A_Plugins_14);
        view_tie_icon_animation(menu->view, item->icon);
        item->index            = index;
        item->callback         = callback;
        item->callback_context = context;
    }, true);
}

void menu_reset(Menu* menu) {
    furi_check(menu);
    with_view_model(menu->view, MenuModel* model, {
        for M_EACH(item, model->items, MenuItemArray_t) {
            icon_animation_stop(item->icon);
            icon_animation_free(item->icon);
        }
        MenuItemArray_reset(model->items);
        model->position = 0;
    }, true);
}

void menu_set_selected_item(Menu* menu, uint32_t index) {
    furi_check(menu);
    with_view_model(menu->view, MenuModel* model, {
        if(index < MenuItemArray_size(model->items))
            model->position = index;
    }, true);
}

void menu_set_theme(Menu* menu, uint8_t theme) {
    furi_check(menu);
    with_view_model(menu->view, MenuModel* model, {
        model->cached_theme = theme;
    }, true);
}

static void menu_change_position(Menu* menu, size_t new_pos) {
    with_view_model(menu->view, MenuModel* model, {
        size_t count = MenuItemArray_size(model->items);
        if(count > 0) {
            if(model->cached_theme != 1) {
                // Classic: only one animation runs at a time — swap selected item
                icon_animation_stop(MenuItemArray_get(model->items, model->position)->icon);
            }
            // Fox: all animations are already running, just update selection
            model->position = new_pos % count;
            if(model->cached_theme != 1) {
                icon_animation_start(MenuItemArray_get(model->items, model->position)->icon);
            }
        }
    }, true);
}

static void menu_process_up(Menu* menu) {
    size_t pos = 0, count = 0;
    uint8_t theme = 1;
    with_view_model(menu->view, MenuModel* model, {
        pos   = model->position;
        count = MenuItemArray_size(model->items);
        theme = model->cached_theme;
    }, false);
    if(!count) return;

    size_t new_pos;
    if(theme == 1) {
        // Toggle row within column (even = top, odd = bottom)
        new_pos = (pos % FOX_ROWS == 0) ?
            ((pos + 1 < count) ? pos + 1 : pos) : pos - 1;
    } else {
        new_pos = (pos > 0) ? pos - 1 : count - 1;
    }
    menu_change_position(menu, new_pos);
}

static void menu_process_down(Menu* menu) {
    size_t pos = 0, count = 0;
    uint8_t theme = 1;
    with_view_model(menu->view, MenuModel* model, {
        pos   = model->position;
        count = MenuItemArray_size(model->items);
        theme = model->cached_theme;
    }, false);
    if(!count) return;

    size_t new_pos;
    if(theme == 1) {
        new_pos = (pos % FOX_ROWS == 0) ?
            ((pos + 1 < count) ? pos + 1 : pos) : pos - 1;
    } else {
        new_pos = (pos + 1 < count) ? pos + 1 : 0;
    }
    menu_change_position(menu, new_pos);
}

static void menu_process_left(Menu* menu) {
    size_t pos = 0, count = 0;
    uint8_t theme = 1;
    with_view_model(menu->view, MenuModel* model, {
        pos   = model->position;
        count = MenuItemArray_size(model->items);
        theme = model->cached_theme;
    }, false);
    if(!count) return;

    size_t new_pos;
    if(theme == 1) {
        if(pos >= FOX_ROWS) {
            new_pos = pos - FOX_ROWS;
        } else {
            size_t last_col_start = ((count - 1) / FOX_ROWS) * FOX_ROWS;
            new_pos = last_col_start + (pos % FOX_ROWS);
            if(new_pos >= count) new_pos = last_col_start;
        }
    } else {
        new_pos = (pos > 0) ? pos - 1 : count - 1;
    }
    menu_change_position(menu, new_pos);
}

static void menu_process_right(Menu* menu) {
    size_t pos = 0, count = 0;
    uint8_t theme = 1;
    with_view_model(menu->view, MenuModel* model, {
        pos   = model->position;
        count = MenuItemArray_size(model->items);
        theme = model->cached_theme;
    }, false);
    if(!count) return;

    size_t new_pos;
    if(theme == 1) {
        size_t candidate = pos + FOX_ROWS;
        new_pos = (candidate < count) ? candidate : (pos % FOX_ROWS);
    } else {
        new_pos = (pos + 1 < count) ? pos + 1 : 0;
    }
    menu_change_position(menu, new_pos);
}

static void menu_process_ok(Menu* menu) {
    MenuItem* item = NULL;
    with_view_model(menu->view, MenuModel* model, {
        if(MenuItemArray_size(model->items))
            item = MenuItemArray_get(model->items, model->position);
    }, true);
    if(item && item->callback)
        item->callback(item->callback_context, item->index);
}
