#include "fox_esp_flasher.h"

static FlasherApp* s_app = NULL;

typedef struct {
    uint8_t  progress;
    char     status[FLASHER_STATUS_LEN];
} ProgressModel;

#define BAR_X  4
#define BAR_Y  13
#define BAR_W  120
#define BAR_H  12
#define BAR_R  2

#define BTN_X  4
#define BTN_Y  50
#define BTN_W  120
#define BTN_H  12
#define BTN_R  3

static void progress_draw(Canvas* canvas, void* model_ptr) {
    ProgressModel* m = model_ptr;
    FlasherApp* app = s_app;

    bool live = app && app->flashing_active;
    bool failed = app && !app->last_result_success;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    const char* header = live ? "Uploading..." : (failed ? "Flash Failed" : "Flash Complete!");
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, header);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, BAR_X, BAR_Y, BAR_W, BAR_H, BAR_R);

    uint8_t max_fill = BAR_W - 2;
    uint8_t filled = (uint8_t)((uint16_t)max_fill * m->progress / 100);
    if(filled > max_fill) filled = max_fill;
    if(filled > 0) {
        canvas_draw_box(canvas, BAR_X + 1, BAR_Y + 1, filled, BAR_H - 2);
    }

    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", m->progress);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, (BAR_X + 1 + filled > 64) ? ColorWhite : ColorBlack);
    canvas_draw_str_aligned(canvas, 64, BAR_Y + BAR_H / 2, AlignCenter, AlignCenter, pct);
    canvas_set_color(canvas, ColorBlack);

    char status_short[27];
    strncpy(status_short, m->status, sizeof(status_short) - 1);
    status_short[sizeof(status_short) - 1] = '\0';
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, 64, BAR_Y + BAR_H + 5, AlignCenter, AlignTop, status_short);

    const char* subline = live ? "DO NOT DISCONNECT"
                        : failed ? "View Terminal for details"
                                 : "Press Back for Menu";
    canvas_draw_str_aligned(canvas, 64, BAR_Y + BAR_H + 16, AlignCenter, AlignTop, subline);

    flasher_draw_ok_button(canvas, BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R, "View Terminal");
}

static bool progress_input(InputEvent* event, void* context) {
    FlasherApp* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyUp:
    case InputKeyDown:

        return true;
    case InputKeyOk:

        view_terminal_reset_scroll(app->terminal_view);
        flasher_switch_view(app, FlasherViewTerminal);
        return true;
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
        m->progress  = 0;
        m->status[0] = '\0';
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
