/**
 * @file loading.c
 * @brief FoxFW custom loading animation — comet tail spinner.
 *
 * HOW REDRAWS WORK IN THE VIEWHOLDER CONTEXT
 * -------------------------------------------
 * The Loader uses a ViewHolder (not a ViewDispatcher) to show this view.
 * ViewHolder listens for the View's update_callback and calls view_port_update
 * on its internal viewport when it fires — that's what actually repaints.
 *
 * view_commit_model(view, false)  — does NOT fire update_callback → no repaint
 * view_commit_model(view, true)   — fires update_callback → ViewHolder repaints
 *
 * The timer therefore calls view_commit_model(view, true) every 50ms, which
 * fires the update_callback, which triggers the ViewHolder to repaint, which
 * calls our draw callback with the new frame. The comet-tail animates.
 *
 * This is also the same mechanism that icon_animation uses internally:
 * view_icon_animation_callback fires the view's update_callback.
 */

#include <gui/modules/loading.h>
#include <furi.h>

#define LOADING_INTERVAL_MS 50u

/* Comet-tail: 8 positions at radius 11, clockwise from top */
static const int8_t spin_dx[8] = {  0,  8, 11,  8,  0, -8,-11, -8};
static const int8_t spin_dy[8] = {-11, -8,  0,  8, 11,  8,  0, -8};

struct Loading {
    View*      view;
    FuriTimer* timer;
};

typedef struct {
    uint8_t frame;
} LoadingModel;

static void loading_draw_callback(Canvas* canvas, void* _model) {
    LoadingModel* model = _model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    const uint8_t frame = model->frame;
    for(uint8_t i = 0; i < 8; i++) {
        uint8_t x = (uint8_t)(64 + spin_dx[i]);
        uint8_t y = (uint8_t)(32 + spin_dy[i]);
        uint8_t age = (uint8_t)((8u + frame - i) % 8u);
        if(age == 0)      canvas_draw_disc(canvas, x, y, 3);
        else if(age == 1) canvas_draw_disc(canvas, x, y, 2);
        else if(age == 2) canvas_draw_disc(canvas, x, y, 1);
        else              canvas_draw_dot(canvas, x, y);
    }
}

static void loading_timer_callback(void* ctx) {
    Loading* loading = ctx;
    /* view_commit_model with true fires the update_callback, which the
     * ViewHolder listens for to call view_port_update on its internal
     * viewport. This is the correct way to drive animation from a timer
     * in both ViewHolder and ViewDispatcher contexts. */
    with_view_model(
        loading->view,
        LoadingModel* model,
        { model->frame = (model->frame + 1u) % 8u; },
        true);
}

Loading* loading_alloc(void) {
    Loading* loading = malloc(sizeof(Loading));
    loading->view = view_alloc();
    view_allocate_model(loading->view, ViewModelTypeLocking, sizeof(LoadingModel));
    view_set_draw_callback(loading->view, loading_draw_callback);
    with_view_model(loading->view, LoadingModel* model, { model->frame = 0; }, false);
    loading->timer = furi_timer_alloc(loading_timer_callback, FuriTimerTypePeriodic, loading);
    furi_timer_start(loading->timer, furi_ms_to_ticks(LOADING_INTERVAL_MS));
    return loading;
}

void loading_free(Loading* loading) {
    furi_assert(loading);
    furi_timer_stop(loading->timer);
    furi_timer_free(loading->timer);
    view_free(loading->view);
    free(loading);
}

View* loading_get_view(Loading* loading) {
    furi_assert(loading);
    return loading->view;
}
