#include "scroll_counter_view.h"
#include "fox_chill_icons.h"

#include <furi.h>
#include <stdio.h>

#define SCROLL_COUNTER_REPEAT_MS 333

static App* s_scroll_app = NULL;

static void scroll_maybe_notify_new_best(App* app) {
    if(!app->scroll_counter_notified_new_best &&
       app->scroll_counter_value > app->save.high_score) {
        app->scroll_counter_notified_new_best = true;
        notification_message(app->notifications, &sequence_blink_yellow_100);
    }
}

static void scroll_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_scroll_app;
    if(app == NULL) return;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Fox Chill");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignTop, "Scroll Counter");

    char formatted[16];
    fox_chill_format_commas(app->scroll_counter_value, formatted, sizeof(formatted));

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, formatted);

    int32_t cx = 64;
    int32_t cy = 43;
    canvas_draw_line(canvas, cx - 8, cy - 4, cx, cy + 4);
    canvas_draw_line(canvas, cx, cy + 4, cx + 8, cy - 4);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "Hold to go faster");

    const Icon* icon = &I_ButtonDown_7x4;
    canvas_draw_icon(canvas, 2, 54, icon);
}

static bool scroll_input_cb(InputEvent* event, void* context) {
    App* app = context;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort || event->type == InputTypeLong) {
            fox_chill_save_note_score(app, app->scroll_counter_value);
        }
        return false;
    }

    if(event->key == InputKeyUp || event->key == InputKeyLeft || event->key == InputKeyRight) {
        return true;
    }

    if(event->key != InputKeyDown) return false;

    if(event->type == InputTypeShort) {
        app->scroll_counter_value++;
        app->scroll_counter_last_tick = furi_get_tick();
        scroll_maybe_notify_new_best(app);
        with_view_model(app->scroll_counter_view, uint8_t * _m, { UNUSED(_m); }, true);
        return true;
    }

    if(event->type == InputTypeRepeat) {
        uint32_t now = furi_get_tick();
        if(now - app->scroll_counter_last_tick >= SCROLL_COUNTER_REPEAT_MS) {
            app->scroll_counter_value++;
            app->scroll_counter_last_tick = now;
            scroll_maybe_notify_new_best(app);
            with_view_model(app->scroll_counter_view, uint8_t * _m, { UNUSED(_m); }, true);
        }
        return true;
    }

    return true;
}

View* scroll_counter_view_alloc(App* app) {
    s_scroll_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, scroll_draw_cb);
    view_set_input_callback(v, scroll_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void scroll_counter_view_free(View* view) {
    s_scroll_app = NULL;
    view_free(view);
}

void scroll_counter_view_show(App* app) {
    app->scroll_counter_value = app->save.high_score;
    app->scroll_counter_last_tick = furi_get_tick();
    app->scroll_counter_notified_new_best = false;
    app->current_view = FoxChillViewScrollCounter;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewScrollCounter);
}
