#include "fox_esp_flasher.h"
#include <string.h>

static FlasherApp* s_app = NULL;

typedef struct {
    bool cmd_focused; /* true when [Send Command] button is selected */
} TermModel;

#define BTN_X  4
#define BTN_Y  52
#define BTN_W  120
#define BTN_H  12
#define BTN_R  3

static void terminal_draw(Canvas* canvas, void* model_ptr) {
    TermModel* m = model_ptr;
    FlasherApp* app = s_app;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Terminal");

    canvas_set_font(canvas, FontSecondary);

    const char* log   = app->term_log;
    size_t      log_n = app->term_log_len;

    /* Walk backwards to find the ends of the last two lines */
    char line1[32] = {0};
    char line2[32] = {0};

    if(log_n > 0) {
        size_t end2 = log_n;
        while(end2 > 0 && (log[end2 - 1] == '\n' || log[end2 - 1] == '\r')) end2--;

        size_t start2 = end2;
        while(start2 > 0 && log[start2 - 1] != '\n') start2--;

        size_t len2 = end2 - start2;
        if(len2 > sizeof(line2) - 1) len2 = sizeof(line2) - 1;
        memcpy(line2, log + start2, len2);
        line2[len2] = '\0';

        if(start2 > 0) {
            size_t end1 = start2 - 1;
            while(end1 > 0 && (log[end1 - 1] == '\n' || log[end1 - 1] == '\r')) end1--;
            size_t start1 = end1;
            while(start1 > 0 && log[start1 - 1] != '\n') start1--;
            size_t len1 = end1 - start1;
            if(len1 > sizeof(line1) - 1) len1 = sizeof(line1) - 1;
            memcpy(line1, log + start1, len1);
            line1[len1] = '\0';
        }
    }

    canvas_draw_str(canvas, 4, 22, line1);
    canvas_draw_str(canvas, 4, 33, line2);

    if(app->last_cmd[0]) {
        char hint[28];
        hint[0] = '>';
        hint[1] = ' ';
        strncpy(hint + 2, app->last_cmd, sizeof(hint) - 3);
        hint[sizeof(hint) - 1] = '\0';
        canvas_draw_str_aligned(canvas, 4, 43, AlignLeft, AlignTop, hint);
    }

    canvas_set_color(canvas, ColorBlack);
    if(m->cmd_focused) {
        canvas_draw_rbox(canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R);
        canvas_draw_rframe(canvas, BTN_X + 2, BTN_Y + 2, BTN_W - 4, BTN_H - 4, BTN_R - 1);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, BTN_Y + BTN_H / 2, AlignCenter, AlignCenter, "Send Command");
    canvas_set_color(canvas, ColorBlack);
}

static bool terminal_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
        with_view_model(app->terminal_view, TermModel* m, {
            m->cmd_focused = !m->cmd_focused;
        }, true);
        return true;
    case InputKeyOk: {
        bool focused = false;
        with_view_model(app->terminal_view, TermModel* m, { focused = m->cmd_focused; }, false);
        if(focused) {
            view_dispatcher_send_custom_event(app->view_dispatcher, FlasherEventTerminalCmd);
        }
        return true;
    }
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
    with_view_model(v, TermModel* m, { m->cmd_focused = true; }, false);
    return v;
}

void view_terminal_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_terminal_refresh(View* v) {
    with_view_model(v, TermModel* m, { UNUSED(m); }, true);
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
