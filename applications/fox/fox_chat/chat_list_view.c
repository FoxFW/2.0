#include "chat_list_view.h"

#include <string.h>

static App* s_chat_list_view_app = NULL;

#define CHAT_ROW_HEADER_H 14
#define CHAT_ROW_H        22
#define CHAT_ROW_VIS       2
#define CHAT_ROW_BOX_X      2
#define CHAT_ROW_BOX_W    124
#define CHAT_ROW_INNER_W  116

bool chat_find_username_split(const char* text, size_t* out_name_len) {
    const char* p = strstr(text, ": ");
    if(p == NULL) return false;
    size_t name_len = (size_t)(p - text);
    if(name_len == 0 || name_len > FOX_DEVICE_NAME_MAX) return false;
    *out_name_len = name_len;
    return true;
}

static void
    chat_draw_line_with_bold_prefix(Canvas* canvas, int32_t lx, int32_t cy, const char* line, size_t bold_len) {
    if(bold_len == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, lx, cy, AlignLeft, AlignCenter, line);
        return;
    }

    char bold_part[CHAT_WRAP_LINE_MAX];
    size_t n = bold_len < sizeof(bold_part) - 1 ? bold_len : sizeof(bold_part) - 1;
    memcpy(bold_part, line, n);
    bold_part[n] = '\0';
    const char* rest_part = line + n;

    canvas_set_font(canvas, FontPrimary);
    int32_t bold_w = (int32_t)canvas_string_width(canvas, bold_part);

    canvas_draw_str_aligned(canvas, lx, cy, AlignLeft, AlignCenter, bold_part);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, lx + bold_w, cy, AlignLeft, AlignCenter, rest_part);
}

static size_t fit_chars(Canvas* canvas, const char* text, size_t len, int max_w) {
    if(max_w <= 0) return 0;
    char buf[CHAT_WRAP_LINE_MAX + 8];
    size_t n = 0;
    while(n < len) {
        size_t take = n + 1;
        size_t cap = take < sizeof(buf) - 1 ? take : sizeof(buf) - 1;
        if(cap < take) break; /* text longer than our probe buffer can hold */
        memcpy(buf, text, cap);
        buf[cap] = '\0';
        if((int)canvas_string_width(canvas, buf) > max_w) break;
        n = take;
    }
    return n;
}

size_t chat_wrap_lines(
    Canvas* canvas,
    const char* text,
    int max_w,
    char out_lines[][CHAT_WRAP_LINE_MAX],
    size_t out_capacity) {
    size_t len = strlen(text);
    size_t pos = 0;
    size_t count = 0;

    while(pos < len && count < out_capacity) {
        size_t remaining = len - pos;
        bool last_slot = (count == out_capacity - 1);

        size_t fit = fit_chars(canvas, text + pos, remaining, max_w);
        if(fit == 0) fit = 1; /* never stall on one very-wide character */
        bool more_after = (pos + fit) < len;

        if(last_slot && more_after) {
            /* This line has to carry "..." to show text kept going -
               refit against a narrower width that leaves room for it.
               Built with a direct memcpy + fixed-index ellipsis rather
               than snprintf("%s...", ...): gcc's -Wformat-truncation
               can't statically see that n below is already bounded
               well under CHAT_WRAP_LINE_MAX (it only knows the
               destination's declared size, not this function's actual
               runtime guarantee), and flags a %s truncation risk that
               can't really happen - same class of false-positive as
               fox_esp32_detector's date buffer warning. Side-stepping
               it by not going through a printf-family call at all is
               simpler than fighting the warning. */
            int dots_w = (int)canvas_string_width(canvas, "...");
            int avail = max_w - dots_w;
            size_t fit2 = fit_chars(canvas, text + pos, remaining, avail);
            size_t n = fit2 < CHAT_WRAP_LINE_MAX - 4 ? fit2 : CHAT_WRAP_LINE_MAX - 4;
            memcpy(out_lines[count], text + pos, n);
            out_lines[count][n] = '.';
            out_lines[count][n + 1] = '.';
            out_lines[count][n + 2] = '.';
            out_lines[count][n + 3] = '\0';
            count++;
            break;
        }

        size_t break_at = fit;
        if(more_after && text[pos + fit] != ' ') {
            /* Prefer breaking at the last space within the fitted
               range, same "don't split mid-word unless forced" rule
               main.c's Terminal wrapper uses - but don't backtrack more
               than a third of the line looking for one. */
            size_t min_break = fit / 3;
            for(size_t i = fit; i > min_break; i--) {
                if(text[pos + i - 1] == ' ') {
                    break_at = i - 1;
                    break;
                }
            }
        }

        size_t n = break_at < CHAT_WRAP_LINE_MAX - 1 ? break_at : CHAT_WRAP_LINE_MAX - 1;
        memcpy(out_lines[count], text + pos, n);
        out_lines[count][n] = '\0';
        count++;

        pos += break_at;
        if(pos < len && text[pos] == ' ') pos++;
    }

    return count;
}

static void chat_draw_scroll(Canvas* canvas, size_t total, size_t vis, size_t scroll) {
    if(total <= vis) return;
    int area_h = 64 - CHAT_ROW_HEADER_H;
    int bar_h = (int)(area_h * (int)vis / (int)total);
    if(bar_h < 3) bar_h = 3;
    int bar_y = CHAT_ROW_HEADER_H + (int)(area_h * (int)scroll / (int)total);
    canvas_draw_box(canvas, 125, bar_y, 3, bar_h);
}

static void chat_list_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_chat_list_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Messages");

    if(app->chat_message_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No messages");
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    for(size_t i = app->chat_message_scroll;
        i < app->chat_message_count && (i - app->chat_message_scroll) < CHAT_ROW_VIS;
        i++) {
        const ChatMessage* cm = &app->chat_messages[i];
        size_t row = i - app->chat_message_scroll;
        int ry = CHAT_ROW_HEADER_H + (int)row * CHAT_ROW_H;
        int by = ry + 1;
        int bh = CHAT_ROW_H - 2;
        bool sel = (i == app->chat_message_selected);

        if(sel) {
            canvas_draw_rbox(canvas, CHAT_ROW_BOX_X, by, CHAT_ROW_BOX_W, bh, 3);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_rframe(canvas, CHAT_ROW_BOX_X, by, CHAT_ROW_BOX_W, bh, 3);
        }

        char lines[2][CHAT_WRAP_LINE_MAX];
        size_t line_count = chat_wrap_lines(canvas, cm->text, CHAT_ROW_INNER_W, lines, 2);

        size_t name_len = 0;
        bool has_name = chat_find_username_split(cm->text, &name_len);

        int32_t text_x = CHAT_ROW_BOX_X + 4;
        if(line_count >= 1) {
            size_t line0_len = strlen(lines[0]);
            size_t bold_len = (has_name && name_len <= line0_len) ? name_len : 0;
            chat_draw_line_with_bold_prefix(canvas, text_x, by + 5, lines[0], bold_len);
        }
        if(line_count >= 2) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, text_x, by + 15, AlignLeft, AlignCenter, lines[1]);
        }

        canvas_set_color(canvas, ColorBlack);
    }

    chat_draw_scroll(canvas, app->chat_message_count, CHAT_ROW_VIS, app->chat_message_scroll);
}

static bool chat_list_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(app->chat_message_count == 0) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->chat_message_selected > 0) {
            app->chat_message_selected--;
            if(app->chat_message_selected < app->chat_message_scroll) {
                app->chat_message_scroll = app->chat_message_selected;
            }
        } else {
            app->chat_message_selected = app->chat_message_count - 1;
            app->chat_message_scroll = (app->chat_message_count > CHAT_ROW_VIS) ?
                                            app->chat_message_count - CHAT_ROW_VIS :
                                            0;
        }
        with_view_model(app->chat_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        if(app->chat_message_selected + 1 < app->chat_message_count) {
            app->chat_message_selected++;
            if(app->chat_message_selected >= app->chat_message_scroll + CHAT_ROW_VIS) {
                app->chat_message_scroll = app->chat_message_selected - CHAT_ROW_VIS + 1;
            }
        } else {
            app->chat_message_selected = 0;
            app->chat_message_scroll = 0;
        }
        with_view_model(app->chat_list_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyOk:
    case InputKeyRight:
        app->chat_detail_scroll = 0;
        app->current_view = FoxCommanderViewChatDetail;
        view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewChatDetail);
        return true;
    case InputKeyBack:
    case InputKeyLeft:
        return false;
    default:
        return false;
    }
}

View* chat_list_view_alloc(App* app) {
    s_chat_list_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, chat_list_draw_cb);
    view_set_input_callback(view, chat_list_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void chat_list_view_free(View* view) {
    s_chat_list_view_app = NULL;
    view_free(view);
}

void chat_list_view_show(App* app) {
    if(app->chat_message_count > 0) {
        app->chat_message_selected = app->chat_message_count - 1;
        app->chat_message_scroll = (app->chat_message_count > CHAT_ROW_VIS) ?
                                        app->chat_message_count - CHAT_ROW_VIS :
                                        0;
    } else {
        app->chat_message_selected = 0;
        app->chat_message_scroll = 0;
    }
    app->menu_return_context = app->menu_context;
    app->current_view = FoxCommanderViewChatList;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxCommanderViewChatList);
}
