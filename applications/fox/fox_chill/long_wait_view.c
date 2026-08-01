#include "long_wait_view.h"
#include "fox_chill_icons.h"

#include <furi.h>
#include <stdio.h>

#define LONG_WAIT_START_SECONDS 60
#define LONG_WAIT_TEXT_AREA_H 50

static App* s_long_wait_app = NULL;

static void long_wait_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_long_wait_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    fox_chill_draw_big_words(canvas, &I_PLEASE, &I_WAIT, 0, LONG_WAIT_TEXT_AREA_H);

    char label[24];
    snprintf(label, sizeof(label), "Next (%lu)", (unsigned long)app->long_wait_seconds_left);
    canvas_set_font(canvas, FontSecondary);
    fox_chill_draw_next_button(canvas, label);
}

static bool long_wait_input_cb(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort) return false;

    switch(event->key) {
    case InputKeyOk:
    case InputKeyRight:
        fox_chill_goto_menu(app);
        return true;
    case InputKeyUp:
    case InputKeyDown:
    case InputKeyLeft:
        return true; // no-op
    case InputKeyBack:
        return false;
    default:
        return false;
    }
}

View* long_wait_view_alloc(App* app) {
    s_long_wait_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, long_wait_draw_cb);
    view_set_input_callback(v, long_wait_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void long_wait_view_free(View* view) {
    s_long_wait_app = NULL;
    view_free(view);
}

void long_wait_view_show(App* app) {
    app->long_wait_seconds_left = LONG_WAIT_START_SECONDS;
    fox_chill_save_note_long_wait(app);
    app->current_view = FoxChillViewLongWait;
    view_dispatcher_switch_to_view(app->view_dispatcher, FoxChillViewLongWait);
}

bool long_wait_view_tick(App* app) {
    if(app->long_wait_seconds_left > 0) app->long_wait_seconds_left--;

    notification_message(app->notifications, &sequence_blink_blue_100);

    with_view_model(app->long_wait_view, uint8_t * _m, { UNUSED(_m); }, true);

    return app->long_wait_seconds_left == 0;
}
