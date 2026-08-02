#include "app.h"
#include <string.h>

static UpdaterApp* s_app = NULL;

static uint32_t s_speed_last_tick = 0;
static uint32_t s_speed_last_bytes = 0;
static uint32_t s_speed_bps = 0;
static bool s_confirm_exit = false;

static uint32_t s_extract_last_bytes = 0xFFFFFFFF;
static uint32_t s_extract_last_change_tick = 0;
static uint8_t s_extract_creep_pct = 0;

static void progress_draw(Canvas* canvas, void* model_ptr) {
    UNUSED(model_ptr);
    UpdaterApp* app = s_app;
    if(!app) return;

    if(s_confirm_exit) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Cancel Download?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Press BACK again to EXIT,");
        canvas_draw_str_aligned(
            canvas, 64, 40, AlignCenter, AlignTop, "or any other key to RESUME.");
        return;
    }

    uint32_t bytes = 0;
    uint32_t total = 0;
    char extract_name[UPDATER_STR_LEN];
    bool cancelling = app->cancel_requested;
    uint8_t phase = app->progress_phase;
    furi_mutex_acquire(app->progress_mutex, FuriWaitForever);
    bytes = app->progress_bytes;
    total = app->progress_total;
    snprintf(extract_name, sizeof(extract_name), "%s", app->extract_current_name);
    furi_mutex_release(app->progress_mutex);

    bool indeterminate = (phase == ProgressPhaseVerify || phase == ProgressPhaseInstall);
    bool has_file_progress = indeterminate && total > 0;

    char title[32];
    if(cancelling) {
        snprintf(title, sizeof(title), "Cancelling");
    } else if(phase == ProgressPhaseVerify) {
        if(has_file_progress) {
            snprintf(title, sizeof(title), "Verifying %lu/%lu", (unsigned long)bytes, (unsigned long)total);
        } else {
            snprintf(title, sizeof(title), "Verifying");
        }
    } else if(phase == ProgressPhaseInstall) {
        if(has_file_progress) {
            snprintf(
                title, sizeof(title), "Extracting Files %lu/%lu", (unsigned long)bytes, (unsigned long)total);
        } else {
            snprintf(title, sizeof(title), "Extracting Files...");
        }
    } else {
        snprintf(title, sizeof(title), "Downloading");
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, title);

    const char* name_source = indeterminate ? extract_name : app->download_name;
    if(name_source[0] != '\0') {
        char name_str[32];
        size_t len = strlen(name_source);
        if(len >= sizeof(name_str)) {

            snprintf(name_str, sizeof(name_str), "...%s", name_source + len - (sizeof(name_str) - 4));
        } else {

            snprintf(name_str, sizeof(name_str), "%.31s", name_source);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, name_str);
    }

    if(indeterminate) {
        canvas_draw_rframe(canvas, 8, 26, 112, 14, 2);
        if(has_file_progress) {
            uint32_t now_ms = furi_get_tick();
            if(bytes != s_extract_last_bytes) {
                s_extract_last_bytes = bytes;
                s_extract_last_change_tick = now_ms;
                s_extract_creep_pct = 0;
            } else {
                uint32_t elapsed_ms = now_ms - s_extract_last_change_tick;
                uint8_t creep_steps = (uint8_t)(elapsed_ms / 3000);
                uint8_t max_creep = (uint8_t)(100 / total);
                if(max_creep > 1) max_creep -= 1;
                if(max_creep == 0) max_creep = 1;
                if(creep_steps > max_creep) creep_steps = max_creep;
                s_extract_creep_pct = creep_steps;
            }

            uint8_t pct = (uint8_t)((uint64_t)bytes * 100 / total) + s_extract_creep_pct;
            if(pct > 100) pct = 100;
            uint8_t fill_w = (uint8_t)(108 * pct / 100);
            if(fill_w > 0) canvas_draw_box(canvas, 10, 28, fill_w, 10);
        } else {
            const uint16_t block_w = 24;
            const uint16_t travel = 108 - block_w;
            const uint32_t cycle_ms = 3200;
            uint32_t t = furi_get_tick() % cycle_ms;
            uint16_t pos;
            if(t < cycle_ms / 2) {
                pos = (uint16_t)((uint64_t)t * travel / (cycle_ms / 2));
            } else {
                pos = (uint16_t)(travel - (uint64_t)(t - cycle_ms / 2) * travel / (cycle_ms / 2));
            }
            canvas_draw_box(canvas, 10 + pos, 28, block_w, 10);
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "This will take 1-2 mins");
        return;
    }

    uint8_t pct = 0;
    if(total > 0) {
        pct = (uint8_t)((uint64_t)bytes * 100 / total);
        if(pct > 100) pct = 100;
    }

    uint32_t now = furi_get_tick();
    if(s_speed_last_tick != 0 && bytes >= s_speed_last_bytes) {
        uint32_t delta_ms = now - s_speed_last_tick;
        if(delta_ms >= 100) {
            uint32_t delta_bytes = bytes - s_speed_last_bytes;
            uint32_t instant_bps = (uint32_t)((uint64_t)delta_bytes * 1000 / delta_ms);
            s_speed_bps = (s_speed_bps * 3 + instant_bps) / 4;
            s_speed_last_bytes = bytes;
            s_speed_last_tick = now;
        }
    } else {

        s_speed_last_bytes = bytes;
        s_speed_last_tick = now;
        s_speed_bps = 0;
    }

    canvas_draw_rframe(canvas, 8, 26, 112, 14, 2);
    uint8_t fill_w = (uint8_t)(108 * pct / 100);
    if(fill_w > 0) canvas_draw_box(canvas, 10, 28, fill_w, 10);

    char pct_str[24];
    if(s_speed_bps > 0) {
        snprintf(pct_str, sizeof(pct_str), "%u%%  %lu KB/s", pct, (unsigned long)(s_speed_bps / 1024));
    } else {
        snprintf(pct_str, sizeof(pct_str), "%u%%", pct);
    }
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

    if(s_confirm_exit) {

        if(event->key == InputKeyBack) {
            s_confirm_exit = false;
            app->cancel_requested = true;
        } else {
            s_confirm_exit = false;
        }
        return true;
    }

    if(event->key == InputKeyBack) {
        if(app->progress_phase != ProgressPhaseDownload) {
            return true;
        }
        s_confirm_exit = true;
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

void view_progress_reset(View* v) {
    UNUSED(v);
    s_speed_last_tick = 0;
    s_speed_last_bytes = 0;
    s_speed_bps = 0;
    s_confirm_exit = false;
    s_extract_last_bytes = 0xFFFFFFFF;
    s_extract_last_change_tick = 0;
    s_extract_creep_pct = 0;
}
