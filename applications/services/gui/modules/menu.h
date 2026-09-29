#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Menu Menu;

typedef void (*MenuItemCallback)(void* context, uint32_t index);

Menu* menu_alloc(void);

void menu_free(Menu* menu);

View* menu_get_view(Menu* menu);

void menu_add_item(
    Menu* menu,
    const char* label,
    const Icon* icon,
    uint32_t index,
    MenuItemCallback callback,
    void* context);

void menu_reset(Menu* menu);

void menu_set_selected_item(Menu* menu, uint32_t index);

void menu_set_theme(Menu* menu, uint8_t theme);

#ifdef __cplusplus
}
#endif
