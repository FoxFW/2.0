#pragma once
/* Reference only - copy fox_scroll_text.c/.h into each app that uses it
   (see fox_file_browser, fox_esp32_terminal). Not linked via a relative
   sources= path. */

#include <stdbool.h>
#include <stdint.h>
#include <gui/canvas.h>

typedef struct {
    int32_t tick;
} FoxScrollText;

void fox_scroll_text_reset(FoxScrollText* state);

void fox_scroll_text_tick(FoxScrollText* state);

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
