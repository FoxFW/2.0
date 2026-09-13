#include "chat_save_view.h"

#include <notification/notification_messages.h>
#include <string.h>
#include <stdio.h>

#define CHAT_SAVE_RESULT_AUTO_RETURN_MS 1800

static App* s_chat_save_result_view_app = NULL;

static void chat_save_result_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_chat_save_result_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 22, AlignCenter, AlignCenter, app->chat_save_result_line1);

    if(app->chat_save_result_line2[0] != '\0') {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 38, AlignCenter, AlignCenter, app->chat_save_result_line2);
    }
}

static bool chat_save_result_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        chat_save_result_view_dismiss(app);
        return true;
    }
    /* Back is intentionally left unhandled here - it bubbles up to
     * fox_chat's shared navigation_callback (main.c), matching every
     * other view in this app (chat_list_view, chat_detail_view,
     * terminal, message_view all do the same for InputKeyBack). */
    return false;
}

static void chat_save_result_timer_cb(void* context) {
    chat_save_result_view_dismiss((App*)context);
}

View* chat_save_result_view_alloc(App* app) {
    s_chat_save_result_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, chat_save_result_draw_cb);
    view_set_input_callback(view, chat_save_result_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));

    app->chat_save_result_timer =
        furi_timer_alloc(chat_save_result_timer_cb, FuriTimerTypeOnce, app);

    return view;
}

void chat_save_result_view_free(View* view) {
    if(s_chat_save_result_view_app != NULL) {
        furi_timer_stop(s_chat_save_result_view_app->chat_save_result_timer);
        furi_timer_free(s_chat_save_result_view_app->chat_save_result_timer);
    }
    s_chat_save_result_view_app = NULL;
    view_free(view);
}

void chat_save_result_view_dismiss(App* app) {
    furi_timer_stop(app->chat_save_result_timer);
    app_switch_to_menu(app, app->menu_return_context);
}

void chat_save_result_view_show(App* app, bool success, const char* line1, const char* line2) {
    notification_message(app->notifications, success ? &sequence_success : &sequence_error);

    strncpy(app->chat_save_result_line1, line1, sizeof(app->chat_save_result_line1) - 1);
    app->chat_save_result_line1[sizeof(app->chat_save_result_line1) - 1] = '\0';

    if(line2 != NULL) {
        strncpy(app->chat_save_result_line2, line2, sizeof(app->chat_save_result_line2) - 1);
        app->chat_save_result_line2[sizeof(app->chat_save_result_line2) - 1] = '\0';
    } else {
        app->chat_save_result_line2[0] = '\0';
    }

    app->menu_return_context = app->menu_context;
    app->current_view = FoxCommanderViewChatSaveResult;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewChatSaveResult);
    furi_timer_start(
        app->chat_save_result_timer, furi_ms_to_ticks(CHAT_SAVE_RESULT_AUTO_RETURN_MS));
}
