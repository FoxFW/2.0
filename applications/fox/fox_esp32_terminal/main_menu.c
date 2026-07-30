#include "main_menu.h"

static App* s_main_menu_app = NULL;

static const char* const s_main_menu_labels[MainMenuItemCount] = {
    [MainMenuViewTerminal] = "View Terminal",
    [MainMenuSendCommand] = "Send Command",
    [MainMenuViewLogs] = "View Logs",
};

#define MAIN_MENU_HEADER_H 14
#define MAIN_MENU_ROW_H    22
#define MAIN_MENU_ROW_VIS  2

static void main_menu_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_main_menu_app;
    if(app == NULL || canvas == NULL) return;

    if(app->main_menu_selected >= MainMenuItemCount) app->main_menu_selected = 0;
    if(app->main_menu_scroll >= MainMenuItemCount) app->main_menu_scroll = 0;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Fox ESP32 Terminal");

    for(size_t i = app->main_menu_scroll;
        i < MainMenuItemCount && (i - app->main_menu_scroll) < MAIN_MENU_ROW_VIS;
        i++) {
        int row = (int)(i - app->main_menu_scroll);
        int ry = MAIN_MENU_HEADER_H + row * MAIN_MENU_ROW_H;
        int by = ry + 1;
        int bh = MAIN_MENU_ROW_H - 2;
        bool selected = (i == app->main_menu_selected);

        if(selected) {
            canvas_draw_rbox(canvas, 2, by, 120, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, by, 120, bh, 3);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, by + bh / 2, AlignCenter, AlignCenter, s_main_menu_labels[i]);

        canvas_set_color(canvas, ColorBlack);
    }

    if(MainMenuItemCount > MAIN_MENU_ROW_VIS) {
        int available_h = 64 - MAIN_MENU_HEADER_H;
        int bar_h = (int)(available_h * MAIN_MENU_ROW_VIS / (int)MainMenuItemCount);
        if(bar_h < 3) bar_h = 3;
        int bar_y = MAIN_MENU_HEADER_H +
                    (int)(available_h * (int)app->main_menu_scroll / (int)MainMenuItemCount);
        canvas_draw_box(canvas, 125, bar_y, 3, bar_h);
    }
}

static void main_menu_activate(App* app, size_t index) {
    if(app == NULL) return;
    switch((MainMenuIndex)index) {
    case MainMenuViewTerminal:
        app_show_terminal(app);
        break;
    case MainMenuSendCommand:
        app_show_send_command(app, false);
        break;
    case MainMenuViewLogs:
        app_show_log_list(app);
        break;
    default:
        break;
    }
}

static bool main_menu_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(app == NULL || event == NULL) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(app->main_menu_selected >= MainMenuItemCount) app->main_menu_selected = 0;
    if(app->main_menu_scroll >= MainMenuItemCount) app->main_menu_scroll = 0;

    switch(event->key) {
    case InputKeyUp:
        if(app->main_menu_selected > 0) {
            app->main_menu_selected--;
            if(app->main_menu_selected < app->main_menu_scroll) {
                app->main_menu_scroll = app->main_menu_selected;
            }
        } else {
            app->main_menu_selected = MainMenuItemCount - 1;
            app->main_menu_scroll = (MainMenuItemCount > MAIN_MENU_ROW_VIS) ?
                                         MainMenuItemCount - MAIN_MENU_ROW_VIS :
                                         0;
        }
        if(app->main_menu_view != NULL) {
            with_view_model(app->main_menu_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyDown:
        if(app->main_menu_selected + 1 < MainMenuItemCount) {
            app->main_menu_selected++;
            if(app->main_menu_selected >= app->main_menu_scroll + MAIN_MENU_ROW_VIS) {
                app->main_menu_scroll = app->main_menu_selected - MAIN_MENU_ROW_VIS + 1;
            }
        } else {
            app->main_menu_selected = 0;
            app->main_menu_scroll = 0;
        }
        if(app->main_menu_view != NULL) {
            with_view_model(app->main_menu_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
    case InputKeyRight:
        main_menu_activate(app, app->main_menu_selected);
        return true;
    case InputKeyLeft:
        return false;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* main_menu_view_alloc(App* app) {
    s_main_menu_app = app;
    View* view = view_alloc();
    if(view == NULL) return NULL;
    view_set_draw_callback(view, main_menu_draw_cb);
    view_set_input_callback(view, main_menu_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void main_menu_view_free(View* view) {
    s_main_menu_app = NULL;
    if(view != NULL) view_free(view);
}

void app_switch_to_main_menu(App* app) {
    if(app == NULL) return;
    app->main_menu_selected = 0;
    app->main_menu_scroll = 0;
    app->current_view = FoxTerminalViewMainMenu;
    if(app->view_dispatcher != NULL) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewMainMenu);
    }
}
