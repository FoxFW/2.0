#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ButtonPanel ButtonPanel;

typedef void (*ButtonItemCallback)(void* context, uint32_t index, InputType type);

ButtonPanel* button_panel_alloc(void);

void button_panel_free(ButtonPanel* button_panel);

void button_panel_reset(ButtonPanel* button_panel);

void button_panel_reset_selection(ButtonPanel* button_panel);

void button_panel_reserve(ButtonPanel* button_panel, size_t reserve_x, size_t reserve_y);

void button_panel_add_item(
    ButtonPanel* button_panel,
    uint32_t index,
    uint16_t matrix_place_x,
    uint16_t matrix_place_y,
    uint16_t x,
    uint16_t y,
    const Icon* icon_name,
    const Icon* icon_name_selected,
    ButtonItemCallback callback,
    void* callback_context);

View* button_panel_get_view(ButtonPanel* button_panel);

void button_panel_add_label(
    ButtonPanel* button_panel,
    uint16_t x,
    uint16_t y,
    Font font,
    const char* label_str);

void button_panel_add_icon(
    ButtonPanel* button_panel,
    uint16_t x,
    uint16_t y,
    const Icon* icon_name);

#ifdef __cplusplus
}
#endif
