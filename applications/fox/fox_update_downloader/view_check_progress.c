#include "app.h"

static UpdaterApp* s_app = NULL;
static bool s_auto_advance_fired = false;

typedef struct {
    uint8_t display_pct;
    char label[32];
    bool await_next;
} CheckProgressModel;

static void check_progress_draw(Canvas* canvas, void* model_ptr) {
    CheckProgressModel* m = model_ptr;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Checking for Updates");

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 8, 22, 112, 14, 2);
    uint8_t fill_w = (uint8_t)(108 * m->display_pct / 100);
    if(fill_w > 0) canvas_draw_box(canvas, 10, 24, fill_w, 10);

    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%u%%", m->display_pct);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, (10 + fill_w > 64) ? ColorWhite : ColorBlack);
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, pct_str);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignTop, m->label);

    if(m->await_next) {
        updater_draw_ok_button(canvas, 26, 48, 76, 14, 3, "Next");
    }
}

static bool check_progress_input(InputEvent* event, void* context) {
    UpdaterApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk && app->check_stage_await_next) {
        view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventCheckProgressNext);
        return true;
    }
    return false;
}

static void check_progress_timer_cb(void* context) {
    UpdaterApp* app = context;
    view_check_progress_refresh(app->check_progress_view);
}

View* view_check_progress_alloc(UpdaterApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, check_progress_draw);
    view_set_input_callback(v, check_progress_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(CheckProgressModel));
    with_view_model(
        v,
        CheckProgressModel * m,
        {
            m->display_pct = 0;
            m->label[0] = '\0';
            m->await_next = false;
        },
        false);
    app->check_progress_timer = furi_timer_alloc(check_progress_timer_cb, FuriTimerTypePeriodic, app);
    return v;
}

void view_check_progress_free(View* v) {
    if(s_app && s_app->check_progress_timer) {
        furi_timer_stop(s_app->check_progress_timer);
        furi_timer_free(s_app->check_progress_timer);
        s_app->check_progress_timer = NULL;
    }
    s_app = NULL;
    view_free(v);
}

void view_check_progress_reset(View* v) {
    s_auto_advance_fired = false;
    with_view_model(
        v,
        CheckProgressModel * m,
        {
            m->display_pct = 0;
            m->label[0] = '\0';
            m->await_next = false;
        },
        true);
}

void view_check_progress_refresh(View* v) {
    UpdaterApp* app = s_app;
    if(!app) return;

    uint8_t target;
    char label[32];
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    target = app->check_stage_target_pct;
    snprintf(label, sizeof(label), "%s", app->check_stage_label);
    furi_mutex_release(app->progress_mutex);

    bool should_advance = false;
    with_view_model(
        v,
        CheckProgressModel * m,
        {
            if(m->display_pct < target) {
                uint8_t step = (uint8_t)((target - m->display_pct) / 6 + 1);
                m->display_pct = (uint8_t)(m->display_pct + step);
                if(m->display_pct > target) m->display_pct = target;
            } else if(m->display_pct > target) {
                m->display_pct = target;
            }
            snprintf(m->label, sizeof(m->label), "%s", label);

            m->await_next = app->check_stage_await_next && m->display_pct >= target;
            if(m->await_next && !s_auto_advance_fired) {
                s_auto_advance_fired = true;
                should_advance = true;
            }
        },
        true);

    if(should_advance) {

        view_dispatcher_send_custom_event(app->view_dispatcher, UpdaterEventCheckProgressNext);
    }
}
