#include "fox_esp_flasher.h"

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t  progress; /* 0-100, snapshot from worker state */
    char     status[FLASHER_STATUS_LEN];
    bool     term_focused; /* when true, [View Terminal] button is selected */
} ProgressModel;

#define BAR_X  4
#define BAR_Y  22
#define BAR_W  120
#define BAR_H  8
#define BAR_R  2

#define BTN_X  4
#define BTN_Y  50
#define BTN_W  120
#define BTN_H  14
#define BTN_R  3

static void progress_draw(Canvas* canvas, void* model_ptr) {
    ProgressModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Uploading...");

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, BAR_X, BAR_Y, BAR_W, BAR_H, BAR_R);

    uint8_t filled = (uint8_t)((BAR_W - 2) * m->progress / 100);
    if(filled > 0) {
        canvas_draw_rbox(canvas, BAR_X + 1, BAR_Y + 1, filled, BAR_H - 2, BAR_R);
    }

    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", m->progress);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, BAR_Y + BAR_H + 2, AlignCenter, AlignTop, pct);

    char status_short[27];
    strncpy(status_short, m->status, sizeof(status_short) - 1);
    status_short[sizeof(status_short) - 1] = '\0';
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignTop, status_short);

    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "DO NOT DISCONNECT");

    canvas_set_color(canvas, ColorBlack);
    if(m->term_focused) {
        canvas_draw_rbox(canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R);
        canvas_draw_rframe(canvas, BTN_X + 2, BTN_Y + 2, BTN_W - 4, BTN_H - 4, BTN_R - 1);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, BTN_Y + BTN_H / 2, AlignCenter, AlignCenter, "View Terminal");
    canvas_set_color(canvas, ColorBlack);
}

static bool progress_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:
        with_view_model(app->progress_view, ProgressModel* m, {
            m->term_focused = !m->term_focused;
        }, true);
        return true;
    case InputKeyOk: {
        bool focused = false;
        with_view_model(app->progress_view, ProgressModel* m, { focused = m->term_focused; }, false);
        if(focused) {
            view_dispatcher_switch_to_view(app->view_dispatcher, FlasherViewTerminal);
        }
        return true;
    }
    case InputKeyBack:
        return true;
    default:
        return false;
    }
}

View* view_progress_alloc(FlasherApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, progress_draw);
    view_set_input_callback(v, progress_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(ProgressModel));
    with_view_model(v, ProgressModel* m, {
        m->progress     = 0;
        m->status[0]    = '\0';
        m->term_focused = false;
    }, false);
    return v;
}

void view_progress_free(View* v) {
    s_app = NULL;
    view_free(v);
}

void view_progress_refresh(View* v) {
    FlasherApp* app = s_app;
    if(!app) return;

    furi_mutex_acquire(app->worker_state.mutex, FuriWaitForever);
    uint8_t pct = app->worker_state.progress;
    char status_snap[FLASHER_STATUS_LEN];
    snprintf(status_snap, sizeof(status_snap), "%s", app->worker_state.status);
    furi_mutex_release(app->worker_state.mutex);

    with_view_model(v, ProgressModel* m, {
        m->progress = pct;
        snprintf(m->status, sizeof(m->status), "%s", status_snap);
    }, true);
}
