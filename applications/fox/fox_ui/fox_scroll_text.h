#pragma once
/**
 * fox_scroll_text — reusable bouncing-scroll text helper for FoxFW FAPs.
 *
 * SHARED LOCATION: applications/fox/fox_ui/fox_scroll_text.h
 *
 * To use in any app, add to application.fam:
 *   sources=["*.c*", "../fox_ui/fox_scroll_text.c"],
 * and in the .c file:
 *   #include "../fox_ui/fox_scroll_text.h"
 *
 * Usage:
 *   1. Add FoxScrollText to the app struct.
 *   2. Allocate a FuriTimer at 50 ms; call fox_scroll_text_tick() in its callback.
 *   3. Call fox_scroll_text_reset() whenever the selected item changes.
 *   4. In the draw callback call fox_scroll_text_draw() — the function centres
 *      text that fits, and bounce-scrolls text that overflows.  The box border
 *      is redrawn internally so no extra call is needed from the caller.
 */

#include <stdbool.h>
#include <stdint.h>
#include <gui/canvas.h>

typedef struct {
    int32_t tick; /**< incremented by fox_scroll_text_tick() every timer fire */
} FoxScrollText;

/** Reset to the start of the animation (call when selected item changes). */
void fox_scroll_text_reset(FoxScrollText* state);

/** Advance the animation by one frame (call from the 50 ms timer callback). */
void fox_scroll_text_tick(FoxScrollText* state);

/**
 * Draw text inside a bounding box with bounce-scrolling if it overflows.
 *
 * @param canvas    Target canvas.
 * @param bx        Box left edge (screen coords) — includes any border.
 * @param by        Box top edge.
 * @param bw        Box width (total, including border on both sides).
 * @param bh        Box height.
 * @param text_y    Global canvas Y at which the text is vertically centred.
 * @param margin    Gap in pixels between the box edge and the text area edge.
 *                  Use 3-4 for a comfortable visual breathing room.
 * @param selected  true = box has a solid fill (rbox); false = outline only
 *                  (rframe).  Controls the colour used for the inner mask so
 *                  the fill is preserved rather than overwritten.
 * @param text      NUL-terminated string to draw.
 * @param state     Bounce-scroll state (must not be NULL).
 *
 * Overflow is masked with the appropriate colour for each region so the fill
 * looks correct.  The box outline is redrawn at the end of the function.
 */
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
    FoxScrollText* state);
