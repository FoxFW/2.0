#include "fox_scroll_text.h"

#define FST_PAUSE_TICKS  10

#define FST_PX_PER_TICK   1

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
    (void)selected;
    if(!canvas || !text || !state || margin < 0) return;

    int tx = bx + margin;
    int tw = bw - 2 * margin;
    if(tw <= 0) return;

    int tw_measured = (int)canvas_string_width(canvas, text);

    if(tw_measured <= tw) {
                canvas_draw_str_aligned(canvas, tx + tw / 2, text_y,
                                AlignCenter, AlignCenter, text);
        return;
    }

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

    canvas_draw_str_aligned(canvas, tx - sx, text_y,
                            AlignLeft, AlignCenter, text);

    canvas_set_color(canvas, ColorWhite);
    if(bx > 0)
        canvas_draw_box(canvas, 0, by, bx, bh);
    int dx = bx + bw;
    if(dx < 128)
        canvas_draw_box(canvas, dx, by, 128 - dx, bh);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, by, bw, bh, FST_BORDER_RADIUS);
}
