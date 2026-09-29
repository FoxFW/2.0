#pragma once

#include <input/input.h>
#include "canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ViewPort ViewPort;

typedef enum {
    ViewPortOrientationHorizontal,
    ViewPortOrientationHorizontalFlip,
    ViewPortOrientationVertical,
    ViewPortOrientationVerticalFlip,
    ViewPortOrientationMAX,
} ViewPortOrientation;

typedef void (*ViewPortDrawCallback)(Canvas* canvas, void* context);

typedef void (*ViewPortInputCallback)(InputEvent* event, void* context);

ViewPort* view_port_alloc(void);

void view_port_free(ViewPort* view_port);

void view_port_set_width(ViewPort* view_port, uint8_t width);
uint8_t view_port_get_width(const ViewPort* view_port);

void view_port_set_height(ViewPort* view_port, uint8_t height);
uint8_t view_port_get_height(const ViewPort* view_port);

void view_port_enabled_set(ViewPort* view_port, bool enabled);
bool view_port_is_enabled(const ViewPort* view_port);

void view_port_draw_callback_set(ViewPort* view_port, ViewPortDrawCallback callback, void* context);
void view_port_input_callback_set(
    ViewPort* view_port,
    ViewPortInputCallback callback,
    void* context);

void view_port_update(ViewPort* view_port);

void view_port_set_orientation(ViewPort* view_port, ViewPortOrientation orientation);
ViewPortOrientation view_port_get_orientation(const ViewPort* view_port);

#ifdef __cplusplus
}
#endif
