#include "download_progress_view.h"
#include "download.h"
#include <string.h>
#include <stdio.h>

static App* s_app = NULL;

static uint32_t s_speed_last_tick = 0;
static uint32_t s_speed_last_bytes = 0;
static uint32_t s_speed_bps = 0;
static bool s_confirm_exit = false;

static bool s_waiting_popup_shown = false;

static bool s_skip_shown = false;
static uint32_t s_skip_shown_tick = 0;

typedef enum {
    ProgressPhaseConnecting,
    ProgressPhasePendingInstall,
    ProgressPhaseDownloading,
    ProgressPhaseComplete,
} ProgressPhase;

static ProgressPhase current_phase(App* app) {
    if(app->download_purpose == DownloadPurposeFile && !app->download_confirmed) {
        return app->download_connected ? ProgressPhasePendingInstall : ProgressPhaseConnecting;
    }

    bool done = false, ok = false;
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    done = app->download_progress_done;
    ok = app->download_progress_ok;
    furi_mutex_release(app->download_progress_mutex);

    if(done && ok && app->download_purpose != DownloadPurposeCatalogPage &&
       app->download_purpose != DownloadPurposeGithubRepoInfo &&
       app->download_purpose != DownloadPurposeGithubTree) {
        return ProgressPhaseComplete;
    }
    return ProgressPhaseDownloading;
}

static void draw_truncated(
    Canvas* canvas,
    int32_t y,
    const char* text,
    size_t max_chars,
    bool truncate_left) {
    char buf[40];
    if(max_chars >= sizeof(buf)) max_chars = sizeof(buf) - 1;
    size_t len = strlen(text);
    if(len > max_chars && max_chars > 3) {
        if(truncate_left) {
            size_t tail = max_chars - 3;
            snprintf(buf, max_chars + 1, "...%s", text + len - tail);
        } else {
            size_t head = max_chars - 3;
            memcpy(buf, text, head);
            memcpy(buf + head, "...", 4);
        }
    } else {
        snprintf(buf, sizeof(buf), "%.*s", (int)max_chars, text);
    }
    canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, buf);
}

static void draw_connecting_title(Canvas* canvas, App* app, bool cancelling, const char* verb) {
    uint8_t attempt = 0, max_attempts = 0;
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    attempt = app->download_progress_attempt;
    max_attempts = app->download_progress_max_attempts;
    furi_mutex_release(app->download_progress_mutex);

    canvas_set_font(canvas, FontPrimary);
    if(cancelling) {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Cancelling");
        return;
    }
    char title_str[28];

    if(attempt > 1) {
        snprintf(title_str, sizeof(title_str), "%s (Try %u/%u)", verb, attempt, max_attempts);
    } else {
        snprintf(title_str, sizeof(title_str), "%s", verb);
    }
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, title_str);
}

static void draw_connecting(Canvas* canvas, App* app) {
    bool cancelling = app->download_cancel_requested;
    draw_connecting_title(canvas, app, cancelling, "Connecting");
    if(cancelling) return;

    if(app->download_url[0] != '\0') {

        char name[40];
        url_derive_filename(app->download_url, name, sizeof(name));
        canvas_set_font(canvas, FontSecondary);
        draw_truncated(canvas, 15, name, 31, false);
    }

    uint32_t elapsed = furi_get_tick() - app->download_connect_attempt_tick;
    uint32_t pct = (elapsed >= FOX_DOWNLOAD_CONNECT_TIMEOUT_MS) ?
                       100 :
                       (elapsed * 100 / FOX_DOWNLOAD_CONNECT_TIMEOUT_MS);
    canvas_draw_rframe(canvas, 8, 26, 112, 14, 2);
    uint8_t fill_w = (uint8_t)(108 * pct / 100);
    if(fill_w > 0) canvas_draw_box(canvas, 10, 28, fill_w, 10);
}

static void draw_pending_install(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Connected");

    canvas_set_font(canvas, FontSecondary);
    draw_truncated(canvas, 13, app->download_found_name, 31, false);

    bool suspicious = app->download_found_type_suspicious;
    if(suspicious) {
        canvas_draw_str_aligned(
            canvas, 64, 22, AlignCenter, AlignTop, "Looks like a web page, not a");
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignTop, "file - check the URL?");
    } else {
        char size_str[24];
        if(app->download_found_size > 0) {
            snprintf(
                size_str, sizeof(size_str), "%lu KB", (unsigned long)(app->download_found_size / 1024));
        } else {
            snprintf(size_str, sizeof(size_str), "size unknown");
        }
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, size_str);
    }

    canvas_draw_rframe(canvas, 8, 39, 112, 6, 1);
    canvas_draw_box(canvas, 9, 40, 110, 4);

    int32_t bar_y = 48;
    int32_t btn_gap = 4;
    int32_t btn_w = (128 - btn_gap * 3) / 2;
    int32_t left_x = btn_gap;
    int32_t right_x = btn_gap * 2 + btn_w;
    bool focus_left = app->download_found_focus_left;

    canvas_set_color(canvas, ColorBlack);
    if(focus_left) {
        canvas_draw_rbox(canvas, left_x, bar_y, btn_w, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, left_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Cancel");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, bar_y, btn_w, 16, 3);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Download");
    } else {
        canvas_draw_rframe(canvas, left_x, bar_y, btn_w, 16, 3);
        canvas_draw_str_aligned(canvas, left_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Cancel");
        canvas_draw_rbox(canvas, right_x, bar_y, btn_w, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(
            canvas, right_x + btn_w / 2, bar_y + 8, AlignCenter, AlignCenter, "Download");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_downloading(Canvas* canvas, App* app) {
    uint32_t bytes = 0;
    uint32_t total = 0;
    bool cancelling = app->download_cancel_requested;
    furi_mutex_acquire(app->download_progress_mutex, FuriWaitForever);
    bytes = app->download_progress_bytes;
    total = app->download_progress_total;
    furi_mutex_release(app->download_progress_mutex);

    draw_connecting_title(canvas, app, cancelling, "Downloading");

    if(app->download_found_name[0] != '\0') {

        canvas_set_font(canvas, FontSecondary);
        draw_truncated(canvas, 15, app->download_found_name, 31, false);
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

    canvas_set_font(canvas, FontSecondary);
    char pct_str[24];
    if(total == 0) {
        snprintf(pct_str, sizeof(pct_str), "%lu KB", (unsigned long)(bytes / 1024));
    } else if(s_speed_bps > 0) {
        snprintf(pct_str, sizeof(pct_str), "%u%%  %lu KB/s", pct, (unsigned long)(s_speed_bps / 1024));
    } else {
        snprintf(pct_str, sizeof(pct_str), "%u%%", pct);
    }
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, pct_str);

    char size_str[48];
    size_str[0] = '\0';
    if(total > 0) {
        snprintf(
            size_str,
            sizeof(size_str),
            "%lu / %lu KB",
            (unsigned long)(bytes / 1024),
            (unsigned long)(total / 1024));
    } else if(bytes > 0) {

        snprintf(size_str, sizeof(size_str), "%lu KB / ?", (unsigned long)(bytes / 1024));
    }
    if(size_str[0] != '\0') {
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, size_str);
    }
}

static void path_display_folder(const char* path, char* out, size_t out_size) {
    out[0] = '\0';
    const char* p = path;
    if(strncmp(p, "/ext/", 5) == 0) p += 5;
    const char* last_slash = strrchr(p, '/');
    if(!last_slash) return;

    const char* seg = p;
    for(const char* q = p; q < last_slash; q++) {
        if(*q == '/') seg = q + 1;
    }
    size_t len = (size_t)(last_slash - seg);
    if(len >= out_size) len = out_size - 1;
    memcpy(out, seg, len);
    out[len] = '\0';
    if(out[0] >= 'a' && out[0] <= 'z') out[0] -= 32;
}

static void draw_complete(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Complete");

    canvas_draw_rframe(canvas, 8, 26, 112, 14, 2);
    canvas_draw_box(canvas, 10, 28, 108, 10);

    char folder[24];
    path_display_folder(app->download_path, folder, sizeof(folder));
    const char* base = strrchr(app->download_path, '/');
    base = base ? base + 1 : app->download_path;

    canvas_set_font(canvas, FontSecondary);
    if(folder[0] != '\0') {

        char line1[48];
        snprintf(line1, sizeof(line1), "SD Card / %s /", folder);
        draw_truncated(canvas, 44, line1, 31, false);
        draw_truncated(canvas, 53, base, 31, false);
    } else {
        draw_truncated(canvas, 48, base, 31, false);
    }
}

static void draw_spinner(Canvas* canvas, int32_t cx, int32_t cy) {
    static const int8_t dx[8] = {0, 6, 8, 6, 0, -6, -8, -6};
    static const int8_t dy[8] = {-8, -6, 0, 6, 8, 6, 0, -6};
    uint8_t frame = (uint8_t)((furi_get_tick() / 100) % 8);
    canvas_set_color(canvas, ColorBlack);
    for(uint8_t i = 0; i < 8; i++) {
        int32_t x = cx + dx[i];
        int32_t y = cy + dy[i];
        uint8_t age = (uint8_t)((8u + frame - i) % 8u);
        if(age == 0) {
            canvas_draw_disc(canvas, x, y, 2);
        } else if(age == 1) {
            canvas_draw_disc(canvas, x, y, 1);
        } else {
            canvas_draw_dot(canvas, x, y);
        }
    }
}

static void draw_waiting_for_esp32(Canvas* canvas) {
    canvas_clear(canvas);
    draw_spinner(canvas, 64, 16);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, "Waiting for ESP32...");
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, "OK: Skip");
}

static void draw_skip_dismiss(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, "Not waiting for ESP32 to");
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "return READY status may");
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "cause errors.");
}

static void progress_draw(Canvas* canvas, void* model_ptr) {
    UNUSED(model_ptr);
    App* app = s_app;
    if(!app) return;

    if(s_skip_shown) {
        draw_skip_dismiss(canvas);
        return;
    }
    if(s_waiting_popup_shown) {
        draw_waiting_for_esp32(canvas);
        return;
    }

    ProgressPhase phase = current_phase(app);

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

    canvas_clear(canvas);
    switch(phase) {
    case ProgressPhaseConnecting:
        draw_connecting(canvas, app);
        break;
    case ProgressPhasePendingInstall:
        draw_pending_install(canvas, app);
        break;
    case ProgressPhaseDownloading:
        draw_downloading(canvas, app);
        break;
    case ProgressPhaseComplete:
        draw_complete(canvas, app);
        break;
    }
}

static bool progress_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    if(s_skip_shown) {

        if(event->key == InputKeyBack || event->key == InputKeyOk) {
            s_skip_shown = false;
            s_waiting_popup_shown = false;
            app_switch_to_menu(app, app->menu_return_context);
        }
        return true;
    }

    if(s_waiting_popup_shown) {
        if(event->key == InputKeyOk) {
            s_skip_shown = true;
            s_skip_shown_tick = furi_get_tick();
            download_progress_view_refresh(app->download_progress_view);
        }
        return true;
    }

    if(s_confirm_exit) {
        if(event->key == InputKeyBack) {
            s_confirm_exit = false;
            app->download_cancel_requested = true;
            app->download_cancel_requested_tick = furi_get_tick();
        } else {
            s_confirm_exit = false;
        }
        return true;
    }

    ProgressPhase phase = current_phase(app);

    if(phase == ProgressPhaseConnecting) {

        if(event->key == InputKeyBack) {
            download_pending_cancel(app);
            return true;
        }
        return false;
    }

    if(phase == ProgressPhasePendingInstall) {
        switch(event->key) {
        case InputKeyBack:
            download_pending_cancel(app);
            return true;
        case InputKeyLeft:
            if(!app->download_found_focus_left) {
                app->download_found_focus_left = true;
                download_progress_view_refresh(app->download_progress_view);
            }
            return true;
        case InputKeyRight:
            if(app->download_found_focus_left) {
                app->download_found_focus_left = false;
                download_progress_view_refresh(app->download_progress_view);
            }
            return true;
        case InputKeyOk:
            if(app->download_found_focus_left) {
                download_pending_cancel(app);
            } else {
                download_install_confirm(app);
            }
            return true;
        default:
            return false;
        }
    }

    if(phase == ProgressPhaseComplete) {

        if(event->key == InputKeyBack || event->key == InputKeyOk) {
            app_switch_to_menu(app, app->menu_return_context);
            return true;
        }
        return false;
    }

    if(event->key == InputKeyBack) {
        s_confirm_exit = true;
        return true;
    }
    return false;
}

static void progress_timer_cb(void* context) {
    App* app = context;

    if(app->download_cancel_requested && !s_waiting_popup_shown && !s_skip_shown) {
        uint32_t elapsed = furi_get_tick() - app->download_cancel_requested_tick;
        if(elapsed >= 1000) {
            s_waiting_popup_shown = true;
        }
    }

    if(s_skip_shown) {
        uint32_t elapsed = furi_get_tick() - s_skip_shown_tick;
        if(elapsed >= 2500) {

            view_dispatcher_send_custom_event(
                app->view_dispatcher, FOX_DOWNLOADER_EVENT_SKIP_WAIT_TIMEOUT);
            return;
        }
    }

    download_progress_view_refresh(app->download_progress_view);
}

void download_progress_view_clear_wait_popups(void) {
    s_waiting_popup_shown = false;
    s_skip_shown = false;
}

View* download_progress_view_alloc(App* app) {
    s_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, progress_draw);
    view_set_input_callback(v, progress_input);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    app->download_progress_timer = furi_timer_alloc(progress_timer_cb, FuriTimerTypePeriodic, app);
    return v;
}

void download_progress_view_free(View* v) {
    if(s_app && s_app->download_progress_timer) {
        furi_timer_stop(s_app->download_progress_timer);
        furi_timer_free(s_app->download_progress_timer);
        s_app->download_progress_timer = NULL;
    }
    s_app = NULL;
    view_free(v);
}

void download_progress_view_refresh(View* v) {
    with_view_model(v, uint8_t * m, { UNUSED(m); }, true);
}

void download_progress_view_reset(View* v) {
    UNUSED(v);
    s_speed_last_tick = 0;
    s_speed_last_bytes = 0;
    s_speed_bps = 0;
    s_confirm_exit = false;
    s_waiting_popup_shown = false;
    s_skip_shown = false;
    s_skip_shown_tick = 0;
}
