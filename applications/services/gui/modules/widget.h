#pragma once
#include <gui/view.h>
#include "widget_elements/widget_element.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Widget Widget;
typedef struct WidgetElement WidgetElement;

Widget* widget_alloc(void);

void widget_free(Widget* widget);

void widget_reset(Widget* widget);

View* widget_get_view(Widget* widget);

void widget_add_string_multiline_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text);

void widget_add_string_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    Font font,
    const char* text);

void widget_add_text_box_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    Align horizontal,
    Align vertical,
    const char* text,
    bool strip_to_dots);

void widget_add_text_scroll_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const char* text);

void widget_add_button_element(
    Widget* widget,
    GuiButtonType button_type,
    const char* text,
    ButtonCallback callback,
    void* context);

void widget_add_icon_element(Widget* widget, uint8_t x, uint8_t y, const Icon* icon);

#define widget_add_frame_element(widget, x, y, width, height, radius) \
    widget_add_rect_element((widget), (x), (y), (width), (height), (radius), false)

void widget_add_rect_element(
    Widget* widget,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    uint8_t radius,
    bool fill);

void widget_add_circle_element(Widget* widget, uint8_t x, uint8_t y, uint8_t radius, bool fill);

void widget_add_line_element(Widget* widget, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

#ifdef __cplusplus
}
#endif
