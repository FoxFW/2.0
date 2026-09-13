#include "terminal_screen.h"
#include "wrap_render.h"

#include <stdint.h>

static App* s_terminal_view_app = NULL;

#define TERMINAL_BOTTOM_BAR_H 16
#define TERMINAL_POLL_MAX_DRAIN 64

/* Two separate focus-based boxes (filled = focused, outlined = not),
 * matching fox_lab/message_view.c's message_draw_two_buttons() reference -
 * this screen used to draw one continuous inverted bar with no focus
 * concept at all (not even two separate boxes), flagged as "the plainest
 * example" of Pattern A by the 2026-09-13 footer-button audit
 * (FOOTER_BUTTON_AUDIT.md project doc). See terminal_input_cb() below. */
static void
    terminal_draw_two_buttons(Canvas* canvas, bool focus_left, const char* left_label) {
    int32_t bar_y = 64 - TERMINAL_BOTTOM_BAR_H;
    int32_t btn_gap = 4;
    int32_t btn_w = (128 - btn_gap * 3) / 2;
    int32_t left_x = btn_gap;
    int32_t right_x = btn_gap * 2 + btn_w;
    const char* right_label = "Send";

    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
    if(focus_left) {
        canvas_draw_rbox(canvas, left_x, bar_y, btn_w, TERMINAL_BOTTOM_BAR_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas,
            left_x + btn_w / 2,
            bar_y + TERMINAL_BOTTOM_BAR_H / 2,
            AlignCenter,
            AlignCenter,
            left_label);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, bar_y, btn_w, TERMINAL_BOTTOM_BAR_H, 3);
        canvas_draw_str_aligned(
            canvas,
            right_x + btn_w / 2,
            bar_y + TERMINAL_BOTTOM_BAR_H / 2,
            AlignCenter,
            AlignCenter,
            right_label);
    } else {
        canvas_draw_rframe(canvas, left_x, bar_y, btn_w, TERMINAL_BOTTOM_BAR_H, 3);
        canvas_draw_str_aligned(
            canvas,
            left_x + btn_w / 2,
            bar_y + TERMINAL_BOTTOM_BAR_H / 2,
            AlignCenter,
            AlignCenter,
            left_label);
        canvas_draw_rbox(canvas, right_x, bar_y, btn_w, TERMINAL_BOTTOM_BAR_H, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas,
            right_x + btn_w / 2,
            bar_y + TERMINAL_BOTTOM_BAR_H / 2,
            AlignCenter,
            AlignCenter,
            right_label);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void terminal_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_terminal_view_app;
    if(app == NULL || canvas == NULL || app->log == NULL) return;

    const char* text = furi_string_get_cstr(app->log);
    size_t text_len = furi_string_size(app->log);

    wrap_render_draw(
        canvas, "TERMINAL", text, text_len, 64 - TERMINAL_BOTTOM_BAR_H, &app->terminal_scroll);

    terminal_draw_two_buttons(
        canvas, app->terminal_bar_focus_left, app->terminal_paused ? "Resume" : "Pause");
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
        if(event->type == InputTypeShort && !app->terminal_bar_focus_left) {
            app->terminal_bar_focus_left = true;
            if(app->terminal_view != NULL) {
                with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
        }
        return true;
    case InputKeyRight:
        if(event->type == InputTypeShort && app->terminal_bar_focus_left) {
            app->terminal_bar_focus_left = false;
            if(app->terminal_view != NULL) {
                with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
        }
        return true;
    case InputKeyOk:
        if(event->type != InputTypeShort) return true;
        if(app->terminal_bar_focus_left) {
            if(app->terminal_paused) {
                terminal_unpause(app);
            } else {
                app->terminal_paused = true;
            }
            if(app->terminal_view != NULL) {
                with_view_model(app->terminal_view, uint8_t * _m, { UNUSED(_m); }, true);
            }
        } else {
            app_show_send_command(app, true);
        }
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
