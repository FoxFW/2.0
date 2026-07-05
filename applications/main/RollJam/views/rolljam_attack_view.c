#include "rolljam_attack_view.h"
#include <gui/canvas.h>

// ============================================================
// Custom drawing for attack status
// Reserved for future use with a custom View
// Currently the app uses Widget modules instead
// ============================================================

void rolljam_attack_view_draw(Canvas* canvas, AttackViewState* state) {
    canvas_clear(canvas);

    // Title bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 2, AlignCenter, AlignTop, state->phase_text);

    // Separator
    canvas_draw_line(canvas, 0, 14, 128, 14);

    // Status
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 18, AlignCenter, AlignTop, state->status_text);

    // Indicators
    int y = 32;

    if(state->jamming) {
        canvas_draw_str(canvas, 4, y, "JAM: [ACTIVE]");
        // Animated dots could go here
    } else {
        canvas_draw_str(canvas, 4, y, "JAM: [OFF]");
    }
    y += 12;

    if(state->capturing) {
        canvas_draw_str(canvas, 4, y, "RX:  [LISTENING]");
    } else {
        canvas_draw_str(canvas, 4, y, "RX:  [OFF]");
    }
    y += 12;

    // Signal counter
    char buf[32];
    snprintf(buf, sizeof(buf), "Signals: %d / 2", state->signal_count);
    canvas_draw_str(canvas, 4, y, buf);

    // Footer
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 62, AlignCenter, AlignBottom, "[BACK] cancel");
}
