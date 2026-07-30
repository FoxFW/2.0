#pragma once

#include <gui/canvas.h>
#include <stddef.h>
#include <stdint.h>

#define WRAP_RENDER_HEADER_H 10

#define WRAP_RENDER_SCROLL_BOTTOM ((size_t)-1)

void wrap_render_draw(
    Canvas* canvas,
    const char* header,
    const char* text,
    size_t text_len,
    int32_t content_bottom_y,
    size_t* scroll);

void wrap_render_scroll(size_t* scroll, int32_t delta);
