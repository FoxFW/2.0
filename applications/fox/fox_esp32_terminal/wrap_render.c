#include "wrap_render.h"

#include <string.h>

#define WRAP_RENDER_MAX_WRAPPED_LINES 256
#define WRAP_RENDER_MEASURE_BUF_MAX   136

static size_t wrap_render_chars_per_line(Canvas* canvas, int32_t max_width) {
    uint16_t w = canvas_string_width(canvas, "W");
    if(w == 0) w = 6;
    size_t n = (size_t)(max_width / w);
    return n < 4 ? 4 : n;
}

typedef struct {
    uint16_t offset;
    uint16_t length;
} WrapRenderLine;

static size_t wrap_render_wrap_text(
    const char* text,
    size_t text_len,
    size_t chars_per_line,
    WrapRenderLine* out,
    size_t out_capacity) {
    if(text == NULL || out == NULL || out_capacity == 0 || chars_per_line == 0) return 0;

    size_t count = 0;
    size_t line_start = 0;

    while(line_start <= text_len && count < out_capacity) {
        size_t line_end = line_start;
        while(line_end < text_len && text[line_end] != '\n') line_end++;

        if(line_end == line_start) {
            out[count].offset = (uint16_t)line_start;
            out[count].length = 0;
            count++;
        } else {
            size_t pos = line_start;
            while(pos < line_end && count < out_capacity) {
                size_t remaining = line_end - pos;
                size_t take = remaining < chars_per_line ? remaining : chars_per_line;
                size_t chunk_end = pos + take;

                if(take == chars_per_line && chunk_end < line_end) {
                    size_t min_break = pos + (chars_per_line / 3);
                    for(size_t i = chunk_end; i > pos && i > min_break; i--) {
                        if(text[i - 1] == ' ') {
                            chunk_end = i - 1;
                            break;
                        }
                    }
                }

                out[count].offset = (uint16_t)pos;
                out[count].length = (uint16_t)(chunk_end - pos);
                count++;
                pos = chunk_end;
                if(pos < line_end && text[pos] == ' ') pos++;
            }
        }

        if(line_end == text_len) break;
        line_start = line_end + 1;
    }

    return count;
}

void wrap_render_draw(
    Canvas* canvas,
    const char* header,
    const char* text,
    size_t text_len,
    int32_t content_bottom_y,
    size_t* scroll) {
    if(canvas == NULL || scroll == NULL) return;
    if(header == NULL) header = "";
    if(text == NULL) {
        text = "";
        text_len = 0;
    }

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, WRAP_RENDER_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, WRAP_RENDER_HEADER_H / 2, AlignCenter, AlignCenter, header);
    canvas_set_color(canvas, ColorBlack);

    int32_t max_width = 122;
    size_t chars_per_line = wrap_render_chars_per_line(canvas, max_width);

    static WrapRenderLine lines[WRAP_RENDER_MAX_WRAPPED_LINES];
    size_t total =
        wrap_render_wrap_text(text, text_len, chars_per_line, lines, WRAP_RENDER_MAX_WRAPPED_LINES);

    size_t line_height = canvas_current_font_height(canvas);
    if(line_height == 0) line_height = 8;
    int32_t content_top = WRAP_RENDER_HEADER_H + 1;
    int32_t content_height = content_bottom_y - content_top;
    if(content_height < 0) content_height = 0;
    size_t visible_rows = (size_t)content_height / line_height;
    if(visible_rows == 0) visible_rows = 1;

    size_t max_scroll = total > visible_rows ? total - visible_rows : 0;
    if(*scroll > max_scroll) *scroll = max_scroll;

    for(size_t row = 0; row < visible_rows && (*scroll + row) < total; row++) {
        const WrapRenderLine* wl = &lines[*scroll + row];
        char buf[WRAP_RENDER_MEASURE_BUF_MAX];
        size_t n = wl->length < (WRAP_RENDER_MEASURE_BUF_MAX - 1) ? wl->length :
                                                                     (WRAP_RENDER_MEASURE_BUF_MAX - 1);
        if((size_t)wl->offset + n > text_len) {
            n = (text_len > wl->offset) ? (text_len - wl->offset) : 0;
        }
        memcpy(buf, text + wl->offset, n);
        buf[n] = '\0';
        int32_t y = (int32_t)(content_top + (int32_t)(row * line_height) + (int32_t)line_height - 1);
        canvas_draw_str(canvas, 2, y, buf);
    }

    if(total > visible_rows && max_scroll > 0) {
        int32_t bar_x = 126;
        int32_t bar_top = content_top;
        int32_t bar_h = content_height;
        canvas_draw_line(canvas, bar_x, bar_top, bar_x, bar_top + bar_h);

        int32_t dot_h = bar_h * (int32_t)visible_rows / (int32_t)total;
        if(dot_h < 3) dot_h = 3;
        int32_t dot_y = bar_top + (bar_h - dot_h) * (int32_t)*scroll / (int32_t)max_scroll;
        canvas_draw_box(canvas, bar_x - 1, dot_y, 3, dot_h);
    }
}

void wrap_render_scroll(size_t* scroll, int32_t delta) {
    if(scroll == NULL) return;
    if(delta < 0) {
        size_t up = (size_t)(-delta);
        *scroll = (*scroll > up) ? (*scroll - up) : 0;
    } else {
        *scroll += (size_t)delta;
    }
}
