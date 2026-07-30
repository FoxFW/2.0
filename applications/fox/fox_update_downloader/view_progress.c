#include "app.h"
#include <string.h>

static UpdaterApp* s_app = NULL;

static void progress_draw(Canvas* canvas, void* model_ptr) {
    UNUSED(model_ptr);
    UpdaterApp* app = s_app;
    if(!app) return;

    uint32_t bytes = 0;
    uint32_t total = 0;
    bool cancelling = app->cancel_requested;
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    bytes = app->progress_bytes;
    total = app->progress_total;
    furi_mutex_release(app->progress_mutex);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 4, AlignCenter, AlignTop, cancelling ? "Cancelling" : "Downloading");

    if(app->download_name[0] != '\0') {
        char name_str[32];
        size_t len = strlen(app->download_name);
        if(len >= sizeof(name_str)) {
            // Keep the tail of the name (extension), not the head, since
            // that's usually the more identifying part when truncated.
            snprintf(
                name_str,
                sizeof(name_str),
                "...%s",
                app->download_name + len - (sizeof(name_str) - 4));
        } else {
            // Precision literal (one less than the buffer size) lets GCC
            // prove the output can never be truncated, unlike a bare "%s"
            // with a source buffer GCC knows can hold more than name_str.
            snprintf(name_str, sizeof(name_str), "%.31s", app->download_name);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, name_str);
    }

    uint8_t pct = 0;
    if(total > 0) {
        pct = (uint8_t)((uint64_t)bytes * 100 / total);
        if(pct > 100) pct = 100;
    }

    canvas_draw_rframe(canvas, 8, 26, 112, 14, 2);
    uint8_t fill_w = (uint8_t)(108 * pct / 100);
    if(fill_w > 0) canvas_draw_box(canvas, 10, 28, fill_w, 10);

    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%u%%", pct);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, pct_str);

    char size_str[48];
    snprintf(
        size_str,
        sizeof(size_str),
        "%lu / %lu KB",
        (unsigned long)(bytes / 1024),
        (unsigned long)(total / 1024));
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, size_str);
}

static bool progress_input(InputEvent* event, void* context) {
    UpdaterApp* app = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyBack) {
        app->cancel_requested = true;
        return true;
    }
    return false;
}

static void progress_timer_cb(void* context) {
    UpdaterApp* app = context;
    view_progress_refresh(app->progress_view);
}

View* view_progress_alloc(UpdaterApp* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, progress_draw);
    view_set_input_callback(v, progress_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    app->progress_timer = furi_timer_alloc(progress_timer_cb, FuriTimerTypePeriodic, app);
    return v;
}

void view_progress_free(View* v) {
    if(s_app && s_app->progress_timer) {
        furi_timer_stop(s_app->progress_timer);
        furi_timer_free(s_app->progress_timer);
        s_app->progress_timer = NULL;
    }
    s_app = NULL;
    view_free(v);
}

void view_progress_refresh(View* v) {
    with_view_model(v, uint8_t * m, { UNUSED(m); }, true);
}
