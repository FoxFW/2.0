#pragma once

#include <stdint.h>
#include <furi.h>
#include "canvas.h"
#include "icon_animation.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ELEMENTS_MAX_LINES_NUM  (7)
#define ELEMENTS_BOLD_MARKER    '#'
#define ELEMENTS_MONO_MARKER    '*'
#define ELEMENTS_INVERSE_MARKER '!'

void elements_progress_bar(Canvas* canvas, int32_t x, int32_t y, size_t width, float progress);

void elements_progress_bar_with_text(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    float progress,
    const char* text);

void elements_scrollbar_pos(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t height,
    size_t pos,
    size_t total);

void elements_scrollbar(Canvas* canvas, size_t pos, size_t total);

void elements_scrollbar_horizontal(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t pos,
    size_t total);

void elements_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);

void elements_button_left(Canvas* canvas, const char* str);

void elements_button_right(Canvas* canvas, const char* str);

void elements_button_up(Canvas* canvas, const char* str);

void elements_button_down(Canvas* canvas, const char* str);

void elements_button_center(Canvas* canvas, const char* str);

void elements_multiline_text_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text);

void elements_multiline_text(Canvas* canvas, int32_t x, int32_t y, const char* text);

void elements_multiline_text_framed(Canvas* canvas, int32_t x, int32_t y, const char* text);

void elements_slightly_rounded_frame(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height);

void elements_slightly_rounded_box(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height);

void elements_bold_rounded_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);

void elements_bubble(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);

void elements_bubble_str(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const char* text,
    Align horizontal,
    Align vertical);

void elements_string_fit_width(Canvas* canvas, FuriString* string, size_t width);

void elements_scrollable_text_line(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    FuriString* string,
    size_t scroll,
    bool ellipsis);

void elements_scrollable_text_line_str(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    const char* string,
    size_t scroll,
    bool ellipsis,
    bool centered);

void elements_text_box(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    Align horizontal,
    Align vertical,
    const char* text,
    bool strip_to_dots);

void elements_fox_horizontal_menu_item(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    const char* label,
    IconAnimation* icon,
    bool selected);

#ifdef __cplusplus
}
#endif
