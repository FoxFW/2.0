#include <gui/modules/loading.h>
#include <furi.h>

#define LOADING_INTERVAL_MS 50u
#define LOADING_MIN_SHOW_MS 400u

static const int8_t spin_dx[8] = {  0,  8, 11,  8,  0, -8,-11, -8};
static const int8_t spin_dy[8] = {-11, -8,  0,  8, 11,  8,  0, -8};

struct Loading {
    View*      view;
    FuriTimer* timer;
};

typedef struct {
    uint8_t frame;
    uint32_t start_tick;
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

static void loading_enter_callback(void* context) {

    View* view = context;
    with_view_model(
        view,
        LoadingModel* model,
        {
            model->frame = 0;
            model->start_tick = furi_get_tick();
        },
        false);
}

static void loading_exit_callback(void* context) {

    View* view = context;
    uint32_t start_tick;
    with_view_model(
        view,
        LoadingModel* model,
        { start_tick = model->start_tick; },
        false);

    uint32_t elapsed_ticks = (uint32_t)(furi_get_tick() - start_tick);
    uint32_t min_ticks = furi_ms_to_ticks(LOADING_MIN_SHOW_MS);
    if(elapsed_ticks < min_ticks) {
        furi_delay_tick(min_ticks - elapsed_ticks);
    }
}

static void loading_timer_callback(void* ctx) {
    Loading* loading = ctx;

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
    view_set_enter_callback(loading->view, loading_enter_callback);
    view_set_exit_callback(loading->view, loading_exit_callback);
    view_set_context(loading->view, loading->view);
    with_view_model(
        loading->view,
        LoadingModel* model,
        {
            model->frame = 0;

            model->start_tick = furi_get_tick();
        },
        false);
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
