#include "message_limit_view.h"
#include "fox_ai_chat_icons.h"

#include <gui/elements.h>
#include <gui/icon_i.h>
#include <stdio.h>

static App* s_message_limit_view_app = NULL;

static void message_limit_draw_back_button(Canvas* canvas) {
    const char* label = "Back";
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_gap = 3;
    int32_t pad = 10;
    int32_t btn_h = 14;
    int32_t btn_y = 64 - btn_h - 4;
    int32_t group_w = icon->width + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t btn_w = group_w + pad * 2;
    int32_t btn_x = (128 - btn_w) / 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, btn_x, btn_y, btn_w, btn_h, 3);
    canvas_set_color(canvas, ColorWhite);
    int32_t gx = btn_x + pad;
    canvas_draw_icon(canvas, gx, btn_y + (btn_h - icon->height) / 2, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon->width + icon_gap, btn_y + btn_h / 2, AlignLeft, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void message_limit_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_message_limit_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "Message Limit");

    canvas_set_font(canvas, FontSecondary);
    char buf[24];
    snprintf(buf, sizeof(buf), "Wait %lu sec...", (unsigned long)app->message_limit_remaining_sec);
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, buf);

    uint32_t total = AI_CHAT_MESSAGE_LIMIT_SEC;
    uint32_t elapsed = total > app->message_limit_remaining_sec ?
                            total - app->message_limit_remaining_sec :
                            total;
    float progress = total == 0 ? 1.0f : (float)elapsed / (float)total;
    elements_progress_bar(canvas, 14, 36, 100, progress);

    message_limit_draw_back_button(canvas);
}

static bool message_limit_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk || event->key == InputKeyBack) {
        furi_timer_stop(app->message_limit_timer);
        app_switch_to_menu(app);
        return true;
    }
    return false;
}

View* message_limit_view_alloc(App* app) {
    s_message_limit_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, message_limit_draw_cb);
    view_set_input_callback(view, message_limit_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void message_limit_view_free(View* view) {
    s_message_limit_view_app = NULL;
    view_free(view);
}

void message_limit_view_show(App* app, uint32_t remaining_sec) {
    if(remaining_sec > AI_CHAT_MESSAGE_LIMIT_SEC) remaining_sec = AI_CHAT_MESSAGE_LIMIT_SEC;

    app->message_limit_elapsed_offset_sec = AI_CHAT_MESSAGE_LIMIT_SEC - remaining_sec;
    app->message_limit_start_tick = furi_get_tick();
    app->message_limit_remaining_sec = remaining_sec;

    app->current_view = AiChatViewMessageLimit;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewMessageLimit);
    with_view_model(app->message_limit_view, uint8_t * _m, { UNUSED(_m); }, true);

    furi_timer_start(app->message_limit_timer, 250);
}

void message_limit_view_tick(App* app) {
    uint32_t elapsed_ms = furi_get_tick() - app->message_limit_start_tick;
    uint32_t elapsed_sec = app->message_limit_elapsed_offset_sec + elapsed_ms / 1000;

    if(elapsed_sec >= AI_CHAT_MESSAGE_LIMIT_SEC) {
        app->message_limit_remaining_sec = 0;
        furi_timer_stop(app->message_limit_timer);
        app_show_text_input(app, "Message", TextInputPurposeChatMessage);
        return;
    }

    app->message_limit_remaining_sec = AI_CHAT_MESSAGE_LIMIT_SEC - elapsed_sec;
    with_view_model(app->message_limit_view, uint8_t * _m, { UNUSED(_m); }, true);
}
