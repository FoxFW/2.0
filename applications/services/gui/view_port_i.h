#pragma once

#include "gui_i.h"
#include "view_port.h"

struct ViewPort {
    Gui* gui;
    FuriMutex* mutex;
    bool is_enabled;
    ViewPortOrientation orientation;

    uint8_t width;
    uint8_t height;

    ViewPortDrawCallback draw_callback;
    void* draw_callback_context;

    ViewPortInputCallback input_callback;
    void* input_callback_context;
};

void view_port_gui_set(ViewPort* view_port, Gui* gui);

void view_port_draw(ViewPort* view_port, Canvas* canvas);

void view_port_input(ViewPort* view_port, InputEvent* event);
