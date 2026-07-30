#include "log_view_screen.h"
#include "session_log.h"
#include "wrap_render.h"

#include <stdio.h>

static App* s_log_view_app = NULL;

static void log_view_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_log_view_app;
    if(app == NULL || canvas == NULL || app->log_content == NULL) return;

    const char* text = furi_string_get_cstr(app->log_content);
    size_t text_len = furi_string_size(app->log_content);
    wrap_render_draw(canvas, "VIEW LOG", text, text_len, 64, &app->log_content_scroll);
}

static bool log_view_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(app == NULL || event == NULL) return false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        wrap_render_scroll(&app->log_content_scroll, -1);
        if(app->log_content_view != NULL) {
            with_view_model(app->log_content_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyDown:
        wrap_render_scroll(&app->log_content_scroll, 1);
        if(app->log_content_view != NULL) {
            with_view_model(app->log_content_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyLeft:
    case InputKeyRight:
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* log_view_screen_alloc(App* app) {
    s_log_view_app = app;
    View* view = view_alloc();
    if(view == NULL) return NULL;
    view_set_draw_callback(view, log_view_draw_cb);
    view_set_input_callback(view, log_view_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void log_view_screen_free(View* view) {
    s_log_view_app = NULL;
    if(view != NULL) view_free(view);
}

void app_show_log_content(App* app, const char* filename) {
    if(app == NULL || filename == NULL || app->log_content == NULL) return;

    snprintf(app->log_content_filename, FOX_LOG_FILENAME_MAX, "%s", filename);

    furi_string_reset(app->log_content);
    furi_string_cat_printf(app->log_content, "Viewing file: %s\n\n", filename);

    FuriString* file_text = furi_string_alloc();
    if(file_text == NULL) {
        furi_string_cat(app->log_content, "(out of memory)");
    } else if(!log_read_file(app, filename, file_text)) {
        furi_string_cat(app->log_content, "(could not open file)");
    } else {
        furi_string_cat(app->log_content, file_text);
    }
    if(file_text != NULL) furi_string_free(file_text);

    app->log_content_scroll = WRAP_RENDER_SCROLL_BOTTOM;
    app->current_view = FoxTerminalViewLogContent;
    if(app->view_dispatcher != NULL) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxTerminalViewLogContent);
    }
}
