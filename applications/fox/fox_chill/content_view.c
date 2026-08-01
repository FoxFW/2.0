#include "content_view.h"
#include "fox_chill_icons.h"

#include <string.h>

#define OUTER_X 2
#define OUTER_Y 11
#define OUTER_W 124
#define OUTER_H 35

#define TEXT_X 8
#define TEXT_W (OUTER_W - 12)
#define TEXT_Y0 (OUTER_Y + 6)
#define TEXT_Y1 (OUTER_Y + OUTER_H - 4)

#define WRAP_LINE_MAX 40
#define WRAP_LINES_MAX 28

static App* s_content_app = NULL;

static void content_flash_new(App* app) {
    notification_message(app->notifications, &sequence_blink_green_100);
}

static size_t fit_chars(Canvas* canvas, const char* text, size_t len, int32_t max_w) {
    if(max_w <= 0) return 0;
    char buf[WRAP_LINE_MAX + 4];
    size_t n = 0;
    while(n < len) {
        size_t take = n + 1;
        size_t cap = take < sizeof(buf) - 1 ? take : sizeof(buf) - 1;
        if(cap < take) break;
        memcpy(buf, text, cap);
        buf[cap] = '\0';
        if((int32_t)canvas_string_width(canvas, buf) > max_w) break;
        n = take;
    }
    return n;
}

static size_t wrap_text(
    Canvas* canvas,
    const char* text,
    int32_t max_w,
    char out_lines[][WRAP_LINE_MAX],
    size_t out_capacity) {
    size_t len = strlen(text);
    size_t pos = 0;
    size_t count = 0;

    if(len == 0) return 0;

    while(pos < len && count < out_capacity) {
        size_t remaining = len - pos;
        size_t fit = fit_chars(canvas, text + pos, remaining, max_w);
        if(fit == 0) fit = 1;

        size_t break_at = fit;
        bool more_after = (pos + fit) < len;
        if(more_after && text[pos + fit] != ' ') {
            size_t min_break = fit / 3;
            for(size_t i = fit; i > min_break; i--) {
                if(text[pos + i - 1] == ' ') {
                    break_at = i - 1;
                    break;
                }
            }
        }

        size_t n = break_at < WRAP_LINE_MAX - 1 ? break_at : WRAP_LINE_MAX - 1;
        memcpy(out_lines[count], text + pos, n);
        out_lines[count][n] = '\0';
        count++;

        pos += break_at;
        if(pos < len && text[pos] == ' ') pos++;
    }

    return count;
}

static void content_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_content_app;
    if(app == NULL) return;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Fox Chill");

    fox_chill_draw_double_border(canvas, OUTER_X, OUTER_Y, OUTER_W, OUTER_H);

    canvas_set_font(canvas, FontSecondary);

    bool showing_answer = app->content_has_answer && app->content_answer_shown;
    const char* display_text = showing_answer ? app->content_answer : app->content_question;

    static char lines[WRAP_LINES_MAX][WRAP_LINE_MAX];
    size_t total = wrap_text(canvas, display_text, TEXT_W, lines, WRAP_LINES_MAX);

    size_t line_height = canvas_current_font_height(canvas);
    if(line_height == 0) line_height = 8;
    size_t visible_rows = (size_t)(TEXT_Y1 - TEXT_Y0) / line_height;
    if(visible_rows == 0) visible_rows = 1;

    size_t max_scroll = total > visible_rows ? total - visible_rows : 0;
    if(app->content_scroll > max_scroll) app->content_scroll = (uint8_t)max_scroll;

    for(size_t row = 0; row < visible_rows; row++) {
        size_t li = app->content_scroll + row;
        if(li >= total) break;

        int32_t y = TEXT_Y0 + (int32_t)(row * line_height);
        canvas_draw_str_aligned(canvas, TEXT_X, y, AlignLeft, AlignTop, lines[li]);
    }

    if(total > visible_rows) {
        elements_scrollbar_pos(
            canvas,
            OUTER_X + OUTER_W - 3,
            TEXT_Y0,
            TEXT_Y1 - TEXT_Y0,
            app->content_scroll,
            total);
    }

    if(app->content_has_answer) {
        fox_chill_draw_left_pill_button(
            canvas, &I_ButtonLeft_4x7, showing_answer ? "Q" : "Ans", 2);
    }

    fox_chill_draw_next_button(canvas, "Next");
}

static bool content_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyOk:
    case InputKeyRight:
        if(event->type == InputTypeShort) {
            fox_chill_pick_random(app, app->content_kind);
            fox_chill_save_note_read(app, app->content_kind);
            content_flash_new(app);
            with_view_model(app->content_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyLeft:
        if(event->type == InputTypeShort && app->content_has_answer) {
            app->content_answer_shown = !app->content_answer_shown;
            app->content_scroll = 0;
            with_view_model(app->content_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    case InputKeyUp:
        if(app->content_scroll > 0) app->content_scroll--;
        with_view_model(app->content_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyDown:
        app->content_scroll++;
        with_view_model(app->content_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* content_view_alloc(App* app) {
    s_content_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, content_draw_cb);
    view_set_input_callback(v, content_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void content_view_free(View* view) {
    s_content_app = NULL;
    view_free(view);
}

void content_view_show(App* app, ContentKind kind) {
    fox_chill_pick_random(app, kind);
    fox_chill_save_note_read(app, kind);
    content_flash_new(app);
    app->current_view = FoxChillViewContent;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewContent);
}
