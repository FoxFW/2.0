#include "fox_scroll_text.h"

/** Ticks to pause at each end before bouncing (50 ms timer). */
#define FST_PAUSE_TICKS  10      /* 10 × 50 ms = 500 ms pause at each end */
/** Pixels to advance per tick — controls scroll speed.
 *  1 px × 20 fps = 20 px/s (halved from the original 2 px/tick).          */
#define FST_PX_PER_TICK   1
/** Radius passed to canvas_draw_rframe when restoring the box border. */
#define FST_BORDER_RADIUS 3


void fox_scroll_text_reset(FoxScrollText* state) {
    if(state) state->tick = 0;
}

void fox_scroll_text_tick(FoxScrollText* state) {
    if(state) state->tick++;
}

void fox_scroll_text_draw(
    Canvas*        canvas,
    int            bx,
    int            by,
    int            bw,
    int            bh,
    int            text_y,
    int            margin,
    bool           selected,
    const char*    text,
    FoxScrollText* state)
{
    (void)selected;  /* inner-mask approach removed — kept for API compatibility */
    if(!canvas || !text || !state || margin < 0) return;

    /* Text area inside the box (margin px inset from each side) */
    int tx = bx + margin;           /* text area left edge  */
    int tw = bw - 2 * margin;       /* text area width      */
    if(tw <= 0) return;

    int tw_measured = (int)canvas_string_width(canvas, text);

    if(tw_measured <= tw) {
                canvas_draw_str_aligned(canvas, tx + tw / 2, text_y,
                                AlignCenter, AlignCenter, text);
        return;
    }

    /* ── Bounce animation ────────────────────────────────────────────────
     * Four phases per cycle (all distances in pixels):
     *   Phase 1: PAUSE at left end  (sx = 0)
     *   Phase 2: SCROLL right       (sx: 0 → overflow)
     *   Phase 3: PAUSE at right end (sx = overflow)
     *   Phase 4: SCROLL left        (sx: overflow → 0)           */
    int overflow = tw_measured - tw;
    int pause_d  = FST_PAUSE_TICKS * FST_PX_PER_TICK;
    int half     = pause_d + overflow;
    int cycle    = 2 * half;

    int d  = (int)((state->tick * (int32_t)FST_PX_PER_TICK) % cycle);
    int sx;
    if(d < pause_d)             sx = 0;
    else if(d < half)           sx = d - pause_d;
    else if(d < half + pause_d) sx = overflow;
    else                        sx = overflow - (d - half - pause_d);

    /* Draw text at its current scroll position */
    canvas_draw_str_aligned(canvas, tx - sx, text_y,
                            AlignLeft, AlignCenter, text);

    /* ── Clip overflow: mask ONLY the areas strictly outside the box ────
     * We do NOT mask the inner regions (between box border and text area)
     * because rectangular masks destroy the rbox rounded corners.
     * Any text that leaks into the 4 px border zone is on the rbox fill
     * (black for selected) so it blends in; the rframe redrawn below
     * restores the visible border outline on top.
     *
     *   Region A: x=0..(bx-1)      — orange background, always
     *   Region D: x=(bx+bw)..127   — orange background, always          */
    canvas_set_color(canvas, ColorWhite);
    if(bx > 0)
        canvas_draw_box(canvas, 0, by, bx, bh);           /* Region A */
    int dx = bx + bw;
    if(dx < 128)
        canvas_draw_box(canvas, dx, by, 128 - dx, bh);    /* Region D */

    /* Restore the rounded box border — drawn in black so it is visible on
     * the orange background at the corners and overrides any stray text
     * pixel that reached the 1 px border zone.                          */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, by, bw, bh, FST_BORDER_RADIUS);
}
