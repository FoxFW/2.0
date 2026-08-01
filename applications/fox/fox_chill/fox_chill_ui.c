#include "app.h"
#include "fox_chill_icons.h"

#include <stdio.h>
#include <string.h>

void fox_chill_draw_double_border(Canvas* canvas, int32_t x, int32_t y, int32_t w, int32_t h) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, w, h, 3);
    canvas_draw_rframe(canvas, x + 1, y + 1, w - 2, h - 2, 2);
}

void fox_chill_draw_next_button(Canvas* canvas, const char* label) {
    const Icon* icon = &I_ButtonCenter_7x7;
    int32_t icon_w = icon_get_width(icon);
    int32_t icon_h = icon_get_height(icon);
    int32_t icon_gap = 3;
    int32_t pad = 8;
    int32_t btn_h = 13;
    int32_t btn_y = 64 - btn_h - 1;

    canvas_set_font(canvas, FontSecondary);
    int32_t group_w = icon_w + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t btn_w = group_w + pad * 2;
    int32_t btn_x = (128 - btn_w) / 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, btn_x, btn_y, btn_w, btn_h, 3);
    canvas_set_color(canvas, ColorWhite);
    int32_t gx = btn_x + pad;
    canvas_draw_icon(canvas, gx, btn_y + (btn_h - icon_h) / 2, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon_w + icon_gap, btn_y + btn_h / 2, AlignLeft, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

void fox_chill_draw_left_pill_button(
    Canvas* canvas,
    const Icon* icon,
    const char* label,
    int32_t x) {
    int32_t icon_w = icon_get_width(icon);
    int32_t icon_h = icon_get_height(icon);
    int32_t icon_gap = 3;
    int32_t pad = 4;
    int32_t btn_h = 13;
    int32_t btn_y = 64 - btn_h - 1;

    canvas_set_font(canvas, FontSecondary);
    int32_t group_w = icon_w + icon_gap + (int32_t)canvas_string_width(canvas, label);
    int32_t btn_w = group_w + pad * 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, btn_y, btn_w, btn_h, 3);
    canvas_set_color(canvas, ColorWhite);
    int32_t gx = x + pad;
    canvas_draw_icon(canvas, gx, btn_y + (btn_h - icon_h) / 2, icon);
    canvas_draw_str_aligned(
        canvas, gx + icon_w + icon_gap, btn_y + btn_h / 2, AlignLeft, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

void fox_chill_format_commas(uint32_t value, char* out, size_t out_cap) {
    char digits[16];
    snprintf(digits, sizeof(digits), "%lu", (unsigned long)value);
    size_t dlen = 0;
    while(digits[dlen] != '\0') dlen++;

    size_t groups_before_first = dlen % 3;
    if(groups_before_first == 0 && dlen > 0) groups_before_first = 3;

    size_t out_pos = 0;
    size_t d = 0;
    size_t first_group = groups_before_first;
    while(d < dlen && out_pos + 1 < out_cap) {
        out[out_pos++] = digits[d++];
        first_group--;
        if(first_group == 0 && d < dlen) {
            if(out_pos + 1 < out_cap) out[out_pos++] = ',';
            first_group = 3;
        }
    }
    if(out_pos < out_cap) out[out_pos] = '\0';
    else out[out_cap - 1] = '\0';
}

void fox_chill_draw_big_words(
    Canvas* canvas,
    const Icon* icon1,
    const Icon* icon2,
    int32_t area_y,
    int32_t area_h) {
    bool has1 = icon1 != NULL;
    bool has2 = icon2 != NULL;
    int32_t gap = 4;

    if(has1 && has2) {
        int32_t h1 = icon_get_height(icon1);
        int32_t h2 = icon_get_height(icon2);
        int32_t w1 = icon_get_width(icon1);
        int32_t w2 = icon_get_width(icon2);
        int32_t total = h1 + gap + h2;
        int32_t top = area_y + (area_h - total) / 2;
        if(top < area_y) top = area_y;
        if(top + total > area_y + area_h) top = area_y + area_h - total;
        canvas_draw_icon(canvas, (128 - w1) / 2, top, icon1);
        canvas_draw_icon(canvas, (128 - w2) / 2, top + h1 + gap, icon2);
    } else if(has1 || has2) {
        const Icon* icon = has1 ? icon1 : icon2;
        int32_t h = icon_get_height(icon);
        int32_t w = icon_get_width(icon);
        int32_t y = area_y + (area_h - h) / 2;
        if(y < area_y) y = area_y;
        canvas_draw_icon(canvas, (128 - w) / 2, y, icon);
    }
}
