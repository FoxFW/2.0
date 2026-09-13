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
 *
 * MINIMUM VISIBLE DURATION (ANTI-FLICKER)
 * -------------------------------------------
 * Every caller of this shared module (the system Loader on every app
 * launch; view_dispatcher_show_loading(), ViewDispatcher's own generic
 * "please wait" helper that a lot of Fox apps call directly; plus any app
 * that embeds a Loading view some other way) renders the spinner from
 * frame 0 the instant the view first appears. When the underlying work
 * finishes fast (well under a second - e.g. Fox File Browser's SD-card
 * scan on a fast card), the spinner used to be on screen for only a
 * handful of milliseconds before the real content replaced it - not long
 * enough to read as a spinner, just a one-frame flash/flicker. Reported
 * 2026-09 from a user's slowed-down screen recording of opening FFB.
 *
 * An earlier version of this fix blanked the canvas for the first 250ms
 * after the view appeared, on the theory that a load fast enough to
 * finish inside that window would never show anything at all. On real
 * hardware that still read as a brief flash (the blank screen replacing
 * the previous screen is itself a visible transition, and short loads
 * landing just past the 250ms mark got a truncated, barely-there spinner
 * flash right after the blank period). Replaced with a different
 * guarantee instead: once this view is showing, it stays showing for at
 * least LOADING_MIN_SHOW_MS, full stop. There is no blank period and the
 * spinner starts animating immediately, exactly as before this fix
 * existed - the only change is that whoever is about to replace this
 * view with the next one is held up, via the exit callback below, until
 * the minimum has elapsed. A fast load now shows a real, genuinely
 * spinning wheel for a third of a second instead of nothing or a flash;
 * a slow load is completely unaffected since it was already going to be
 * on screen longer than that.
 *
 * "Most recently became current" matters for both the enter and exit
 * callback because both the Loader and view_dispatcher_show_loading()
 * allocate ONE Loading instance and reuse it across every subsequent
 * show - so start_tick can't just be set once in loading_alloc(), or
 * every show after the very first would inherit a start_tick from
 * minutes/hours ago and the minimum-duration check below would never
 * trigger. Reset it instead from an enter_callback, which view_enter()
 * (called by both view_holder_set_view() and
 * view_dispatcher_switch_to_view(), the two ways this view ever becomes
 * current) fires every single time this view is (re-)shown, with zero
 * changes needed at any call site.
 *
 * The exit_callback's blocking wait is safe to do here specifically
 * because view_exit() runs synchronously on whichever thread called
 * view_holder_set_view()/view_dispatcher_set_current_view() to switch
 * away from this view - not on the shared Gui/ViewPort compositor
 * thread - and at the point view_exit() fires, the caller hasn't yet
 * detached this view from its ViewPort (that happens right after
 * view_exit() returns). So while this callback blocks, the frame timer
 * (an independent FuriTimer, unaffected by anything happening on the
 * caller's thread) keeps ticking and keeps calling view_port_update()
 * through the update_callback, and the spinner keeps genuinely animating
 * on screen for the entire wait - it never freezes mid-frame.
 *
 * This callback deliberately does NOT fire during outright teardown -
 * e.g. view_dispatcher_free() frees a still-current loading view
 * directly via loading_free() without ever calling view_exit() on it
 * (see view_dispatcher.c) - so shutting down an app or the whole
 * ViewDispatcher is never held up by this.
 */

#include <gui/modules/loading.h>
#include <furi.h>

#define LOADING_INTERVAL_MS 50u
#define LOADING_MIN_SHOW_MS 400u

/* Comet-tail: 8 positions at radius 11, clockwise from top */
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
    /* Fired by view_enter() every time this view becomes the one on
     * screen - see the file header's "MINIMUM VISIBLE DURATION" comment
     * for why this can't just be a one-time init in loading_alloc(). We
     * set view_set_context() to the View* itself below purely so this
     * callback (and loading_exit_callback() below) have something to
     * hand to with_view_model() - nothing else in this file reads View's
     * generic context field. */
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
    /* Fired by view_exit() every time this view is about to be replaced
     * or hidden - see the file header's "MINIMUM VISIBLE DURATION"
     * comment for the full reasoning. Read start_tick under a brief
     * model lock, released before the wait so the frame timer's own
     * with_view_model() calls are never blocked and the spinner keeps
     * animating on screen for the whole delay. */
    View* view = context;
    uint32_t start_tick;
    with_view_model(
        view,
        LoadingModel* model,
        { start_tick = model->start_tick; },
        false);

    /* Unsigned subtraction is intentional - correct even if
     * furi_get_tick() wraps. */
    uint32_t elapsed_ticks = (uint32_t)(furi_get_tick() - start_tick);
    uint32_t min_ticks = furi_ms_to_ticks(LOADING_MIN_SHOW_MS);
    if(elapsed_ticks < min_ticks) {
        furi_delay_tick(min_ticks - elapsed_ticks);
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
    view_set_enter_callback(loading->view, loading_enter_callback);
    view_set_exit_callback(loading->view, loading_exit_callback);
    view_set_context(loading->view, loading->view);
    with_view_model(
        loading->view,
        LoadingModel* model,
        {
            model->frame = 0;
            /* Defensive default in case draw_callback or exit_callback
             * ever run before this view's first view_enter() (shouldn't
             * happen on either of the two real paths - both call
             * view_enter() before enabling the viewport - but a
             * just-started start_tick is a far safer failure mode than
             * an uninitialized one). The enter_callback above is what
             * actually matters for repeat shows. */
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
