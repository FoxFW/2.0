#include "chat_detail_view.h"
#include "chat_list_view.h" /* chat_wrap_lines / CHAT_WRAP_LINE_MAX */

#include <string.h>
#include <stdio.h>

static App* s_chat_detail_view_app = NULL;

#define CHAT_DETAIL_BOTTOM_BAR_H 16
#define CHAT_DETAIL_INNER_W      122
#define CHAT_DETAIL_LINE_MAX_VIS 10

static void chat_detail_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_chat_detail_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->chat_message_count == 0 || app->chat_message_selected >= app->chat_message_count) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No message");
        return;
    }
    const ChatMessage* cm = &app->chat_messages[app->chat_message_selected];

    static char lines[CHAT_DETAIL_LINE_MAX_VIS][CHAT_WRAP_LINE_MAX];
    size_t total =
        chat_wrap_lines(canvas, cm->text, CHAT_DETAIL_INNER_W, lines, CHAT_DETAIL_LINE_MAX_VIS);

    size_t line_height = canvas_current_font_height(canvas);
    if(line_height == 0) line_height = 10;
    size_t content_top = 2;
    size_t content_height = (64 - CHAT_DETAIL_BOTTOM_BAR_H) - content_top;
    size_t visible_rows = content_height / line_height;
    if(visible_rows == 0) visible_rows = 1;

    size_t max_scroll = total > visible_rows ? total - visible_rows : 0;
    if(app->chat_detail_scroll > max_scroll) app->chat_detail_scroll = max_scroll;

    size_t name_len = 0;
    bool has_name = chat_find_username_split(cm->text, &name_len);

    for(size_t row = 0; row < visible_rows && (app->chat_detail_scroll + row) < total; row++) {
        size_t idx = app->chat_detail_scroll + row;
        int32_t y = (int32_t)(content_top + row * line_height + line_height - 1);

        if(idx == 0 && has_name && name_len <= strlen(lines[0])) {
            char bold_part[CHAT_WRAP_LINE_MAX];
            size_t n = name_len < sizeof(bold_part) - 1 ? name_len : sizeof(bold_part) - 1;
            memcpy(bold_part, lines[0], n);
            bold_part[n] = '\0';
            const char* rest_part = lines[0] + n;

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 3, y, bold_part);
            int32_t bold_w = (int32_t)canvas_string_width(canvas, bold_part);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 3 + bold_w, y, rest_part);
        } else {
            canvas_draw_str(canvas, 3, y, lines[idx]);
        }
    }

    if(total > visible_rows) {
        int32_t bar_x = 126;
        int32_t bar_top = (int32_t)content_top;
        int32_t bar_h = (int32_t)content_height;
        canvas_draw_line(canvas, bar_x, bar_top, bar_x, bar_top + bar_h);

        int32_t dot_h = bar_h * (int32_t)visible_rows / (int32_t)total;
        if(dot_h < 3) dot_h = 3;
        int32_t dot_y =
            bar_top + (bar_h - dot_h) * (int32_t)app->chat_detail_scroll / (int32_t)max_scroll;
        canvas_draw_box(canvas, bar_x - 1, dot_y, 3, dot_h);
    }

    int32_t bar_y = 64 - CHAT_DETAIL_BOTTOM_BAR_H;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, bar_y, 128, CHAT_DETAIL_BOTTOM_BAR_H);
    canvas_set_color(canvas, ColorWhite);
    char bar_text[24];
    snprintf(bar_text, sizeof(bar_text), "Sent %s UTC", cm->time);
    canvas_draw_str_aligned(
        canvas, 64, bar_y + CHAT_DETAIL_BOTTOM_BAR_H / 2, AlignCenter, AlignCenter, bar_text);
    canvas_set_color(canvas, ColorBlack);
}

static bool chat_detail_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->chat_detail_scroll > 0) app->chat_detail_scroll--;
        with_view_model(app->chat_detail_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->chat_detail_scroll++;
        with_view_model(app->chat_detail_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false;
    default:
        return false;
    }
}

View* chat_detail_view_alloc(App* app) {
    s_chat_detail_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, chat_detail_draw_cb);
    view_set_input_callback(view, chat_detail_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void chat_detail_view_free(View* view) {
    s_chat_detail_view_app = NULL;
    view_free(view);
}
