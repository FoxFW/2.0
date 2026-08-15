#include "progress_view.h"

static App* s_progress_view_app = NULL;

static void progress_draw_cb(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = s_progress_view_app;
    if(app == NULL) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    const char* line1 = app->progress_stage == ProgressStageSending ? "Sending Message..." :
                                                                       "Receiving Reply...";
    const char* line2 = "Please wait";

    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, line1);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, line2);
}

static bool progress_input_cb(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    /* the send/receive exchange runs synchronously on the same task that
     * drives input, so there's nothing to route here - block all input
     * while the request is in flight. */
    return true;
}

View* progress_view_alloc(App* app) {
    s_progress_view_app = app;
    View* view = view_alloc();
    view_set_draw_callback(view, progress_draw_cb);
    view_set_input_callback(view, progress_input_cb);
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(uint8_t));
    return view;
}

void progress_view_free(View* view) {
    s_progress_view_app = NULL;
    view_free(view);
}

void progress_view_show(App* app, ProgressStage stage) {
    app->progress_stage = stage;
    app->current_view = AiChatViewProgress;
    view_dispatcher_switch_to_view(app->view_dispatcher, AiChatViewProgress);
    with_view_model(app->progress_view, uint8_t * _m, { UNUSED(_m); }, true);
}

void progress_view_set_stage(App* app, ProgressStage stage) {
    app->progress_stage = stage;
    with_view_model(app->progress_view, uint8_t * _m, { UNUSED(_m); }, true);
}
