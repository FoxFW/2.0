#include "terminal_screen.h"
#include "wrap_render.h"

#include <stdint.h>

static App* s_terminal_view_app = NULL;

#define TERMINAL_BOTTOM_BAR_H 16
#define TERMINAL_POLL_MAX_DRAIN 64

static void terminal_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_terminal_view_app;
    if(app == NULL || canvas == NULL || app->log == NULL) return;

    const char* text = furi_string_get_cstr(app->log);
    size_t text_len = furi_string_size(app->log);

    wrap_render_draw(
        canvas, "TERMINAL", text, text_len, 64 - TERMINAL_BOTTOM_BAR_H, &app->terminal_scroll);

    int32_t bar_y = 64 - TERMINAL_BOTTOM_BAR_H;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, bar_y, 128, TERMINAL_BOTTOM_BAR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        4,
        bar_y + TERMINAL_BOTTOM_BAR_H / 2,
        AlignLeft,
        AlignCenter,
        app->terminal_paused ? "< Resume" : "< Pause");
    canvas_draw_str_aligned(
        canvas, 124, bar_y + TERMINAL_BOTTOM_BAR_H / 2, AlignRight, AlignCenter, "Send >");
    canvas_set_color(canvas, ColorBlack);
}

static bool terminal_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(app == NULL || event == NULL) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        wrap_render_scroll(&app->terminal_scroll, -1);
        if(app->terminal_view != NULL) {
            with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyDown:
        wrap_render_scroll(&app->terminal_scroll, 1);
        if(app->terminal_view != NULL) {
            with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyLeft:
        if(app->terminal_paused) {
            terminal_unpause(app);
        } else {
            app->terminal_paused = true;
        }
        if(app->terminal_view != NULL) {
            with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyRight:
    case InputKeyOk:
        app_show_send_command(app, true);
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* terminal_view_alloc(App* app) {
    s_terminal_view_app = app;
    View* view = view_alloc();
    if(view == NULL) return NULL;
    view_set_draw_callback(view, terminal_draw_cb);
    view_set_input_callback(view, terminal_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void terminal_view_free(View* view) {
    s_terminal_view_app = NULL;
    if(view != NULL) view_free(view);
}

void terminal_unpause(App* app) {
    if(app == NULL) return;
    if(app->terminal_paused_skipped_lines > 0) {
        app_log(
            app,
            "...(%lu line(s) skipped while paused)...",
            (unsigned long)app->terminal_paused_skipped_lines);
        app->terminal_paused_skipped_lines = 0;
    }
    app->terminal_paused = false;
    app->terminal_scroll = WRAP_RENDER_SCROLL_BOTTOM;
}

void terminal_poll_tick(App* app) {
    if(app == NULL || app->esp_at == NULL) return;

    EspAtMsg msg;
    bool got_any = false;
    size_t drained = 0;
    while(drained < TERMINAL_POLL_MAX_DRAIN && esp_at_receive(app->esp_at, &msg, 0)) {
        drained++;
        if(app->terminal_paused) {
            if(app->terminal_paused_skipped_lines < SIZE_MAX) {
                app->terminal_paused_skipped_lines++;
            }
            continue;
        }
        app_log(app, "%s", msg.line);
        got_any = true;
    }

    bool terminal_visible = (app->current_view == FoxTerminalViewTerminal);

    if(got_any && terminal_visible && !app->terminal_paused) {
        app->terminal_scroll = WRAP_RENDER_SCROLL_BOTTOM;
    }

    if(terminal_visible && app->terminal_view != NULL) {
        with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
    }
}
