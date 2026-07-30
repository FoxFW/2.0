#include "fox_esp_flasher.h"
#include <string.h>

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t scroll;
} TermModel;

#define BTN_X  4
#define BTN_Y  52
#define BTN_W  120
#define BTN_H  12
#define BTN_R  3

#define TERM_VISIBLE_LINES 3
#define TERM_LINE_Y0        22
#define TERM_LINE_DY        11
#define TERM_LINE_MAXW      32
#define TERM_MAX_LINES      40

static size_t term_split_lines(
    const char* log,
    size_t      log_n,
    uint16_t*   starts,
    uint16_t*   lens,
    size_t      max_lines) {
    size_t n = 0;
    size_t line_start = 0;
    for(size_t i = 0; i <= log_n; i++) {
        if(i == log_n || log[i] == '\n') {
            size_t end = i;
            while(end > line_start && log[end - 1] == '\r') end--;
            if(!(i == log_n && end == line_start && n > 0)) {
                if(n < max_lines) {
                    starts[n] = (uint16_t)line_start;
                    lens[n]   = (uint16_t)(end - line_start);
                    n++;
                }
            }
            line_start = i + 1;
        }
    }
    return n;
}

static void terminal_draw(Canvas* canvas, void* model_ptr) {
    TermModel* m = model_ptr;
    FlasherApp* app = s_app;
    if(!app) return;

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Terminal");
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);

    const char* log   = app->term_log;
    size_t      log_n = app->term_log_len;

    uint16_t starts[TERM_MAX_LINES];
    uint16_t lens[TERM_MAX_LINES];
    size_t total_lines = term_split_lines(log, log_n, starts, lens, TERM_MAX_LINES);

    bool show_hint = (app->last_cmd[0] && !app->flashing_active && !app->flash_done_pending);
    size_t visible_lines = show_hint ? 2 : TERM_VISIBLE_LINES;

    if(total_lines > 0) {
        uint8_t max_scroll =
            (total_lines > visible_lines) ? (uint8_t)(total_lines - visible_lines) : 0;
        uint8_t scroll = (m->scroll > max_scroll) ? max_scroll : m->scroll;

        size_t bottom_idx = total_lines - 1 - scroll;
        size_t top_idx = (bottom_idx + 1 > visible_lines) ? (bottom_idx + 1 - visible_lines) : 0;

        for(size_t li = top_idx; li <= bottom_idx; li++) {
            char linebuf[TERM_LINE_MAXW];
            size_t len = lens[li];
            if(len > sizeof(linebuf) - 1) len = sizeof(linebuf) - 1;
            memcpy(linebuf, log + starts[li], len);
            linebuf[len] = '\0';
            uint8_t row = (uint8_t)(li - top_idx);
            canvas_draw_str(canvas, 4, TERM_LINE_Y0 + row * TERM_LINE_DY, linebuf);
        }
    }

    if(show_hint) {
        char hint[28];
        hint[0] = '>';
        hint[1] = ' ';
        strncpy(hint + 2, app->last_cmd, sizeof(hint) - 3);
        hint[sizeof(hint) - 1] = '\0';
        canvas_draw_str_aligned(canvas, 4, 43, AlignLeft, AlignTop, hint);
    }

    flasher_draw_ok_button(
        canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R,
        (app->flashing_active || app->flash_done_pending) ? "Back" : "Send Command");
}

static bool terminal_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:

        with_view_model(app->terminal_view, TermModel* m, {
            if(m->scroll < 0xFF) m->scroll++;
        }, true);
        return true;
    case InputKeyDown:
        with_view_model(app->terminal_view, TermModel* m, {
            if(m->scroll > 0) m->scroll--;
        }, true);
        return true;
    case InputKeyOk:

        if(app->flashing_active || app->flash_done_pending) {
            flasher_terminal_back(app);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventTerminalCmd);
        }
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* view_terminal_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, terminal_draw);
    view_set_input_callback(v, terminal_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(TermModel));
    with_view_model(v, TermModel* m, { m->scroll = 0; }, false);
    return v;
}

void view_terminal_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_terminal_refresh(View* v) {
    with_view_model(v, TermModel* m, { UNUSED(m); }, true);
}

void view_terminal_reset_scroll(View* v) {
    with_view_model(v, TermModel* m, { m->scroll = 0; }, true);
}

void view_terminal_append(FlasherApp* app, const char* str, size_t len) {
    if(!str || len == 0) return;
    size_t remaining = FLASHER_TERM_LOG_LEN - 1 - app->term_log_len;
    if(len > remaining) {
        size_t to_drop = len - remaining + 64;
        if(to_drop > app->term_log_len) {
            app->term_log_len = 0;
        } else {
            memmove(app->term_log, app->term_log + to_drop,
                    app->term_log_len - to_drop);
            app->term_log_len -= to_drop;
        }
    }
    size_t to_copy = len;
    if(to_copy > FLASHER_TERM_LOG_LEN - 1 - app->term_log_len) {
        to_copy = FLASHER_TERM_LOG_LEN - 1 - app->term_log_len;
    }
    memcpy(app->term_log + app->term_log_len, str, to_copy);
    app->term_log_len += to_copy;
    app->term_log[app->term_log_len] = '\0';
}
