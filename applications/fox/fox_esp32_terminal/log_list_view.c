#include "log_list_view.h"
#include "session_log.h"
#include "fox_scroll_text.h"

#include <gui/elements.h>
#include <string.h>

static App* s_log_list_app = NULL;

#define LOG_LIST_ROW_HEADER_H 14
#define LOG_LIST_ROW_H        22
#define LOG_LIST_ROW_VIS      2
#define LOG_LIST_NAME_MAX_W   112

#define LOG_LIST_SCROLL_MS 50

static void log_list_scroll_timer_cb(void* context) {
    App* app = context;
    if(app == NULL) return;
    fox_scroll_text_tick(&app->log_list_text_anim);
    if(app->log_list_view != NULL) {
        with_view_model(app->log_list_view, uint8_t * _m, { UNUSED(_m); }, true);
    }
}

static void log_list_scroll_start(App* app) {
    if(app == NULL) return;
    fox_scroll_text_reset(&app->log_list_text_anim);
    if(app->log_list_scroll_timer != NULL) {
        furi_timer_start(app->log_list_scroll_timer, LOG_LIST_SCROLL_MS);
    }
}

void log_list_scroll_stop(App* app) {
    if(app == NULL) return;
    if(app->log_list_scroll_timer != NULL) {
        furi_timer_stop(app->log_list_scroll_timer);
    }
    fox_scroll_text_reset(&app->log_list_text_anim);
}

static void log_list_display_name(const char* filename, char* out, size_t out_size) {
    if(out_size == 0) return;
    snprintf(out, out_size, "%s", filename);
}

static void log_list_draw_name(Canvas* canvas, int32_t cx, int32_t cy, const char* name) {
    if(canvas_string_width(canvas, name) <= LOG_LIST_NAME_MAX_W) {
        canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, name);
        return;
    }

    char buf[FOX_LOG_FILENAME_MAX + 1];
    size_t len = strlen(name);
    if(len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, name, len);
    buf[len] = '\0';

    while(len > 0) {
        buf[len] = '\0';
        char with_ellipsis[FOX_LOG_FILENAME_MAX + 4];
        snprintf(with_ellipsis, sizeof(with_ellipsis), "%s...", buf);
        if(canvas_string_width(canvas, with_ellipsis) <= LOG_LIST_NAME_MAX_W) {
            canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, with_ellipsis);
            return;
        }
        len--;
    }
    canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, "...");
}

static void log_list_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_log_list_app;
    if(app == NULL || canvas == NULL) return;

    if(app->log_file_count > FOX_LOG_FILE_LIST_MAX) app->log_file_count = FOX_LOG_FILE_LIST_MAX;
    if(app->log_file_selected >= app->log_file_count) app->log_file_selected = 0;
    if(app->log_file_scroll >= app->log_file_count) app->log_file_scroll = 0;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "View Logs");

    if(app->log_file_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No log files yet");
        return;
    }

    for(size_t i = app->log_file_scroll;
        i < app->log_file_count && (i - app->log_file_scroll) < LOG_LIST_ROW_VIS;
        i++) {
        int row = (int)(i - app->log_file_scroll);
        int ry = LOG_LIST_ROW_HEADER_H + row * LOG_LIST_ROW_H;
        int by = ry + 1;
        int bh = LOG_LIST_ROW_H - 2;
        bool selected = (i == app->log_file_selected);

        if(selected) {
            canvas_draw_rbox(canvas, 2, by, 120, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, 2, by, 120, bh, 3);
        }

        char display_name[FOX_LOG_FILENAME_MAX];
        log_list_display_name(app->log_files[i], display_name, sizeof(display_name));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, by + 5, AlignCenter, AlignCenter, "File Name");
        canvas_set_font(canvas, FontSecondary);
        if(selected) {
            fox_scroll_text_draw(
                canvas, 2, by, 120, bh, by + 15, 4, true, display_name, &app->log_list_text_anim);
        } else {
            log_list_draw_name(canvas, 64, by + 15, display_name);
        }

        canvas_set_color(canvas, ColorBlack);
    }

    if(app->log_file_count > LOG_LIST_ROW_VIS) {
        // Dotted track + solid position block, matching FOX_CHILL's
        // scrollbar style instead of a plain solid bar with no track.
        int available_h = 64 - LOG_LIST_ROW_HEADER_H;
        elements_scrollbar_pos(
            canvas,
            128,
            LOG_LIST_ROW_HEADER_H,
            (size_t)available_h,
            (size_t)app->log_file_scroll,
            (size_t)app->log_file_count);
    }
}

static bool log_list_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(app == NULL || event == NULL) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->log_file_count == 0) return false;
    if(app->log_file_count > FOX_LOG_FILE_LIST_MAX) app->log_file_count = FOX_LOG_FILE_LIST_MAX;
    if(app->log_file_selected >= app->log_file_count) app->log_file_selected = 0;

    switch(event->key) {
    case InputKeyUp:
        if(app->log_file_selected > 0) {
            app->log_file_selected--;
            if(app->log_file_selected < app->log_file_scroll) {
                app->log_file_scroll = app->log_file_selected;
            }
        } else {
            app->log_file_selected = app->log_file_count - 1;
            app->log_file_scroll = (app->log_file_count > LOG_LIST_ROW_VIS) ?
                                        app->log_file_count - LOG_LIST_ROW_VIS :
                                        0;
        }

        fox_scroll_text_reset(&app->log_list_text_anim);
        if(app->log_list_view != NULL) {
            with_view_model(app->log_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyDown:
        if(app->log_file_selected + 1 < app->log_file_count) {
            app->log_file_selected++;
            if(app->log_file_selected >= app->log_file_scroll + LOG_LIST_ROW_VIS) {
                app->log_file_scroll = app->log_file_selected - LOG_LIST_ROW_VIS + 1;
            }
        } else {
            app->log_file_selected = 0;
            app->log_file_scroll = 0;
        }
        fox_scroll_text_reset(&app->log_list_text_anim);
        if(app->log_list_view != NULL) {
            with_view_model(app->log_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyOk:
    case InputKeyRight:
        if(app->log_file_selected < app->log_file_count) {
            log_list_scroll_stop(app);
            app_show_log_content(app, app->log_files[app->log_file_selected]);
        }
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false;
    default:
        return false;
    }
}

View* log_list_view_alloc(App* app) {
    s_log_list_app = app;
    View* view = view_alloc();
    if(view == NULL) return NULL;
    view_set_draw_callback(view, log_list_draw_cb);
    view_set_input_callback(view, log_list_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    if(app != NULL) {
        app->log_list_scroll_timer =
            furi_timer_alloc(log_list_scroll_timer_cb, FuriTimerTypePeriodic, app);
    }
    return view;
}

void log_list_view_free(View* view) {
    if(s_log_list_app != NULL && s_log_list_app->log_list_scroll_timer != NULL) {
        furi_timer_stop(s_log_list_app->log_list_scroll_timer);
        furi_timer_free(s_log_list_app->log_list_scroll_timer);
        s_log_list_app->log_list_scroll_timer = NULL;
    }
    s_log_list_app = NULL;
    if(view != NULL) view_free(view);
}

void app_show_log_list(App* app) {
    if(app == NULL) return;
    log_list_scan(app);
    app->current_view = FoxTerminalViewLogList;
    if(app->view_dispatcher != NULL) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewLogList);
    }
    log_list_scroll_start(app);
}
