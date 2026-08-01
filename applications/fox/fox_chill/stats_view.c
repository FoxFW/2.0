#include "stats_view.h"

#include <stdio.h>

#define OUTER_X 2
#define OUTER_Y 11
#define OUTER_W 124
#define OUTER_H 47

#define COL_LEFT_X (OUTER_X + 5)
#define COL_RIGHT_X (OUTER_X + OUTER_W / 2 + 3)
#define ROW_Y0 (OUTER_Y + 6)
#define ROW_STEP 10

static App* s_stats_app = NULL;

static void draw_row(Canvas* canvas, int32_t x, int32_t y, const char* label, uint32_t value) {
    char formatted[16];
    fox_chill_format_commas(value, formatted, sizeof(formatted));
    char row[32];
    snprintf(row, sizeof(row), "%s: %s", label, formatted);
    canvas_draw_str_aligned(canvas, x, y, AlignLeft, AlignTop, row);
}

static void stats_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_stats_app;
    if(app == NULL) return;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Fox Chill");

    fox_chill_draw_double_border(canvas, OUTER_X, OUTER_Y, OUTER_W, OUTER_H);

    canvas_set_font(canvas, FontSecondary);

    int32_t y = ROW_Y0;
    draw_row(canvas, COL_LEFT_X, y, "Jokes", app->save.jokes_read);
    draw_row(canvas, COL_RIGHT_X, y, "Yo Mama", app->save.yo_mama_read);
    y += ROW_STEP;
    draw_row(canvas, COL_LEFT_X, y, "Riddles", app->save.riddles_read);
    draw_row(canvas, COL_RIGHT_X, y, "Mindful", app->save.mindful_sessions);
    y += ROW_STEP;
    draw_row(canvas, COL_LEFT_X, y, "Facts", app->save.facts_read);
    draw_row(canvas, COL_RIGHT_X, y, "Waits", app->save.long_waits);
    y += ROW_STEP;
    draw_row(canvas, COL_LEFT_X, y, "Stats", app->save.statistics_read);
    draw_row(canvas, COL_RIGHT_X, y, "Down", app->save.high_score);
}

static bool stats_input_cb(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

View* stats_view_alloc(App* app) {
    s_stats_app = app;
    View* v = view_alloc();
    view_set_draw_callback(v, stats_draw_cb);
    view_set_input_callback(v, stats_input_cb);
    view_set_context(v, app);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(uint8_t));
    return v;
}

void stats_view_free(View* view) {
    s_stats_app = NULL;
    view_free(view);
}
