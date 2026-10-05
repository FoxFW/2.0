#include "splash_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <math.h>
#include <string.h>

/* Boot intro. Nyx is the night goddess; a covert camera has to light the dark
 * to see, so the intro is an eye opening in the dark while IR wave-rings wash
 * over it. It runs ~1.6 s and any key skips it — a splash you cannot skip is a
 * splash people learn to resent.
 *
 * Timeline, in 100 ms ticks:
 *   0..8   the eye opens (lids part, pupil dilates)
 *   4..end wave-rings pulse outward from the pupil
 *   6..end "NYX" wordmark, then the tagline, fade in
 *   >=16   done
 */

#define SPLASH_EYE_CX     64
#define SPLASH_EYE_CY     27
#define SPLASH_EYE_HALF_W 30 // eye half-width
#define SPLASH_EYE_HALF_H 14 // eye half-height when fully open
/* First row the wordmark occupies. FontPrimary sits in [baseline-7 .. baseline]
 * and the wordmark's baseline is 52, so nothing decorative may reach row 44. */
#define SPLASH_TEXT_TOP 42
#define SPLASH_OPEN_TICKS 8
#define SPLASH_DONE_TICKS 16

struct SplashView {
    View* view;
    SplashViewCallback done_cb;
    void* done_ctx;
};

typedef struct {
    uint8_t anim; // frames since the scene entered
} SplashModel;

/* One lid of the almond eye, as a run of short segments. `sign` = -1 upper lid,
 * +1 lower. `open` in 0..1 scales how far the lids have parted. */
static void draw_lid(Canvas* canvas, float open, int sign) {
    int prev_x = 0, prev_y = 0;
    for(int i = 0; i <= 24; i++) {
        float t = (float)i / 24.0f; // 0..1 across the width
        int x = SPLASH_EYE_CX - SPLASH_EYE_HALF_W + (int)(t * 2 * SPLASH_EYE_HALF_W);
        /* sinf gives the almond curve — zero at the corners, max at center. */
        int y = SPLASH_EYE_CY + sign * (int)(sinf(t * (float)M_PI) * SPLASH_EYE_HALF_H * open);
        if(i) canvas_draw_line(canvas, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

/* A ring of dots rather than canvas_draw_circle, because a full circle at these
 * radii sweeps down through the wordmark and the tagline and strikes them
 * through — the rings have to stop where the type starts. Plotting the ring
 * point by point is the only way to clip it; the canvas has no clip region, and
 * the dotted texture suits light spilling outward anyway. */
static void draw_wave_ring(Canvas* canvas, int radius) {
    for(int deg = 0; deg < 360; deg += 7) {
        float t = (float)deg * (float)M_PI / 180.0f;
        int x = SPLASH_EYE_CX + (int)(cosf(t) * (float)radius);
        int y = SPLASH_EYE_CY + (int)(sinf(t) * (float)radius);
        if(y > SPLASH_TEXT_TOP || y < 0 || x < 0 || x > 127) continue;
        canvas_draw_dot(canvas, x, y);
    }
}

static void splash_view_draw(Canvas* canvas, void* model) {
    SplashModel* m = model;
    uint8_t a = m->anim;

    float open = (float)(a < SPLASH_OPEN_TICKS ? a : SPLASH_OPEN_TICKS) / (float)SPLASH_OPEN_TICKS;

    /* ---- IR wave-rings washing out from the pupil ---- */
    if(a >= 4) {
        for(int r = 0; r < 3; r++) {
            int radius = ((a * 4) + r * 10) % 44;
            if(radius > 6) draw_wave_ring(canvas, radius);
        }
    }

    /* ---- the eye ---- */
    draw_lid(canvas, open, -1);
    draw_lid(canvas, open, +1);
    /* corners */
    canvas_draw_dot(canvas, SPLASH_EYE_CX - SPLASH_EYE_HALF_W, SPLASH_EYE_CY);
    canvas_draw_dot(canvas, SPLASH_EYE_CX + SPLASH_EYE_HALF_W, SPLASH_EYE_CY);

    /* iris + pupil dilating as the eye opens */
    if(open > 0.35f) {
        int iris = (int)(7 * open);
        canvas_draw_circle(canvas, SPLASH_EYE_CX, SPLASH_EYE_CY, iris);
        int pupil = (int)(3 * open);
        canvas_draw_disc(canvas, SPLASH_EYE_CX, SPLASH_EYE_CY, pupil > 1 ? pupil : 1);
        /* catch-light, the spark that makes it read as an eye */
        canvas_draw_dot(canvas, SPLASH_EYE_CX + 2, SPLASH_EYE_CY - 2);
    }

    /* ---- wordmark + tagline ---- */
    if(a >= 6) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "N Y X");
    }
    if(a >= 9) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "IR EMITTER SWEEP");
    }
}

static bool splash_view_input(InputEvent* event, void* context) {
    SplashView* v = context;
    /* Any press skips straight to the menu. */
    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        if(v->done_cb) v->done_cb(v->done_ctx);
        return true;
    }
    return false;
}

SplashView* splash_view_alloc(void) {
    SplashView* v = malloc(sizeof(SplashView));
    memset(v, 0, sizeof(SplashView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, splash_view_draw);
    view_set_input_callback(v->view, splash_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SplashModel));
    return v;
}

void splash_view_free(SplashView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* splash_view_get_view(SplashView* v) {
    furi_assert(v);
    return v->view;
}

void splash_view_set_done_callback(SplashView* v, SplashViewCallback cb, void* context) {
    furi_assert(v);
    v->done_cb = cb;
    v->done_ctx = context;
}

void splash_view_reset(SplashView* v) {
    furi_assert(v);
    with_view_model(v->view, SplashModel * m, { m->anim = 0; }, true);
}

bool splash_view_tick(SplashView* v) {
    furi_assert(v);
    bool done = false;
    with_view_model(
        v->view,
        SplashModel * m,
        {
            if(m->anim < 255) m->anim++;
            done = m->anim >= SPLASH_DONE_TICKS;
        },
        true);
    return done;
}
