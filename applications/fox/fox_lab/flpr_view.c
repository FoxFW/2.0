#include "flpr_view.h"
#include "foxr_companion.h"
#include "fox_lab_icons.h"

#include <gui/icon.h>
#include <string.h>

#define FLPR_VIEW_POLL_MS           300
#define FLPR_MAX_VISIBLE_LOG_LINES  3
#define FLPR_HEADER_BAR_H           10
#define FLPR_BOTTOM_BAR_H           16

static App* s_flpr_view_app = NULL;
static FuriTimer* s_flpr_poll_timer = NULL;
static uint32_t s_flpr_last_log_version = 0;

/* Centered pill + I_ButtonCenter_7x7 icon, OK-activated - matches
 * message_view.c's message_draw_one_button() reference exactly. This
 * screen used to draw a full-width inverted bar with left-aligned
 * "< Launcher" text and no icon, activated by Left - flagged as "Pattern
 * C-incorrect" by the 2026-09-13 footer-button audit (FOOTER_BUTTON_AUDIT.md
 * project doc), which is doubly notable since this was the screen
 * originally pointed to as the *reference* example of a correct single
 * button - it wasn't actually built to that style. Fixed to match for
 * real. */
static void flpr_draw_launcher_button(Canvas* canvas) {
    const char* label = "Launcher";
    int32_t bar_y = 64 - FLPR_BOTTOM_BAR_H;
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_w = icon_get_width(icon);
    int32_t icon_h = icon_get_height(icon);
    int32_t icon_gap = 3;
    int32_t pad_x = 10;
    int32_t content_w = icon_w + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t btn_w = content_w + pad_x * 2;
    int32_t x = (128 - btn_w) / 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, bar_y, btn_w, FLPR_BOTTOM_BAR_H, 3);
    canvas_set_color(canvas, ColorWhite);

    int32_t gx = x + (btn_w - content_w) / 2;
    int32_t gy_icon = bar_y + (FLPR_BOTTOM_BAR_H - icon_h) / 2;
    canvas_draw_icon(canvas, gx, gy_icon, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon_w + icon_gap, bar_y + FLPR_BOTTOM_BAR_H / 2, AlignLeft, AlignCenter, label);

    canvas_set_color(canvas, ColorBlack);
}

static void flpr_draw_cb(Canvas* canvas, void* model) {
    /* Mirrors launcher_view.c's own pattern: the model here is a trivial
     * redraw trigger (see flpr_timer_cb below), not where the actual data
     * lives - the log snapshot is read fresh from the companion on every
     * draw instead, via the file-static App* pointer. */
    UNUSED(model);
    App* app = s_flpr_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);

    /* "Fox terminal" look, matching the header/footer-bar formula already
     * established elsewhere in this project for terminal-style screens
     * (fox_esp32_terminal's wrap_render.c/terminal_screen.c: full-width
     * inverted bar, white centered title, top and bottom) - this screen
     * used to be a plain list with no visual relation to those. Fitting
     * both bars in meant trimming this screen's other content (one info
     * line instead of two, 3 visible log lines instead of 4) to the same
     * ~38px band between them. User-requested, 2026-09-11. */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, FLPR_HEADER_BAR_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, FLPR_HEADER_BAR_H / 2, AlignCenter, AlignCenter, "FOXLAB TERMINAL");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "Keep this app open");
    canvas_draw_line(canvas, 2, 21, 125, 21);

    if(app->companion != NULL) {
        char log_lines[FOXR_LOG_LINES][FOXR_LOG_LINE_MAX];
        size_t log_count = 0;
        foxr_companion_log_snapshot(app->companion, log_lines, &log_count);

        if(log_count == 0) {
            canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "(no commands yet)");
        } else {
            size_t start = (log_count > FLPR_MAX_VISIBLE_LOG_LINES) ?
                                (log_count - FLPR_MAX_VISIBLE_LOG_LINES) :
                                0;
            int32_t y = 24;
            for(size_t i = start; i < log_count; i++) {
                canvas_draw_str(canvas, 2, y + 6, log_lines[i]);
                y += 8;
            }
        }
    }

    flpr_draw_launcher_button(canvas);
}

static bool flpr_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        /* Same destination Back already goes to (navigation_callback in
         * main.c) - handled here too so the drawn button is actually
         * live, not just decorative. */
        app_show_launcher(app);
        return true;
    }

    /* Everything else (notably Back) propagates up to the view
     * dispatcher's navigation callback, same as before this screen had any
     * interactive input of its own. */
    return false;
}

static void flpr_timer_cb(void* context) {
    App* app = context;
    if(app->companion == NULL || app->flpr_view == NULL) return;
    uint32_t v = foxr_companion_log_version(app->companion);
    if(v != s_flpr_last_log_version) {
        s_flpr_last_log_version = v;
        with_view_model(app->flpr_view, uint8_t * _m, { UNUSED(_m); }, true);
    }
}

View* flpr_view_alloc(App* app) {
    s_flpr_view_app = app;
    s_flpr_last_log_version = 0;

    View* view = view_alloc();
    view_set_draw_callback(view, flpr_draw_cb);
    view_set_input_callback(view, flpr_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));

    s_flpr_poll_timer = furi_timer_alloc(flpr_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(s_flpr_poll_timer, FLPR_VIEW_POLL_MS);

    return view;
}

void flpr_view_free(View* view) {
    if(s_flpr_poll_timer) {
        furi_timer_stop(s_flpr_poll_timer);
        furi_timer_free(s_flpr_poll_timer);
        s_flpr_poll_timer = NULL;
    }
    s_flpr_view_app = NULL;
    view_free(view);
}
