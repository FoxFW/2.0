#pragma once

#include <stdint.h>
#include <stddef.h>
#include <gui/icon_animation.h>
#include <gui/icon.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ColorWhite = 0x00,
    ColorBlack = 0x01,
    ColorXOR = 0x02,
} Color;

typedef enum {
    FontPrimary,
    FontSecondary,
    FontKeyboard,
    FontBigNumbers,
    FontBatteryPercent,

    FontTotalNumber,
} Font;

typedef enum {
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter,
} Align;

typedef enum {
    CanvasOrientationHorizontal,
    CanvasOrientationHorizontalFlip,
    CanvasOrientationVertical,
    CanvasOrientationVerticalFlip,
} CanvasOrientation;

typedef enum {
    CanvasDirectionLeftToRight,
    CanvasDirectionTopToBottom,
    CanvasDirectionRightToLeft,
    CanvasDirectionBottomToTop,
} CanvasDirection;

typedef struct {
    uint8_t leading_default;
    uint8_t leading_min;
    uint8_t height;
    uint8_t descender;
} CanvasFontParameters;

typedef enum {
    IconFlipNone,
    IconFlipHorizontal,
    IconFlipVertical,
    IconFlipBoth,
} IconFlip;

typedef enum {
    IconRotation0,
    IconRotation90,
    IconRotation180,
    IconRotation270,
} IconRotation;

typedef struct Canvas Canvas;

void canvas_reset(Canvas* canvas);

void canvas_commit(Canvas* canvas);

size_t canvas_width(const Canvas* canvas);

size_t canvas_height(const Canvas* canvas);

size_t canvas_current_font_height(const Canvas* canvas);

size_t canvas_current_font_width(const Canvas* canvas);

const CanvasFontParameters* canvas_get_font_params(const Canvas* canvas, Font font);

void canvas_clear(Canvas* canvas);

void canvas_set_color(Canvas* canvas, Color color);

void canvas_set_font_direction(Canvas* canvas, CanvasDirection dir);

void canvas_invert_color(Canvas* canvas);

void canvas_set_font(Canvas* canvas, Font font);

void canvas_set_custom_u8g2_font(Canvas* canvas, const uint8_t* font);

void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str);

void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* str);

uint16_t canvas_string_width(Canvas* canvas, const char* str);

size_t canvas_glyph_width(Canvas* canvas, uint16_t symbol);

void canvas_draw_bitmap(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    const uint8_t* compressed_bitmap_data);

void canvas_draw_icon_ex(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const Icon* icon,
    IconRotation rotation);

void canvas_draw_icon_animation(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    IconAnimation* icon_animation);

void canvas_draw_icon(Canvas* canvas, int32_t x, int32_t y, const Icon* icon);

void canvas_draw_xbm(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    const uint8_t* bitmap);

void canvas_draw_xbm_ex(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    IconRotation rotation,
    const uint8_t* bitmap_data);

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y);

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);

void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height);

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

void canvas_draw_circle(Canvas* canvas, int32_t x, int32_t y, size_t radius);

void canvas_draw_disc(Canvas* canvas, int32_t x, int32_t y, size_t radius);

void canvas_draw_triangle(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t base,
    size_t height,
    CanvasDirection dir);

void canvas_draw_glyph(Canvas* canvas, int32_t x, int32_t y, uint16_t ch);

void canvas_set_bitmap_mode(Canvas* canvas, bool alpha);

void canvas_draw_rframe(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    size_t radius);

void canvas_draw_rbox(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    size_t radius);

void canvas_draw_icon_bitmap(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    int16_t w,
    int16_t h,
    const Icon* icon);

#ifdef __cplusplus
}
#endif
