#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Submenu Submenu;
typedef void (*SubmenuItemCallback)(void* context, uint32_t index);
typedef void (*SubmenuItemCallbackEx)(void* context, InputType input_type, uint32_t index);

Submenu* submenu_alloc(void);

void submenu_free(Submenu* submenu);

View* submenu_get_view(Submenu* submenu);

void submenu_add_item(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallback callback,
    void* callback_context);

void submenu_add_lockable_item(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallback callback,
    void* callback_context,
    bool locked,
    const char* locked_message);

void submenu_add_item_ex(
    Submenu* submenu,
    const char* label,
    uint32_t index,
    SubmenuItemCallbackEx callback,
    void* callback_context);

void submenu_change_item_label(Submenu* submenu, uint32_t index, const char* label);

void submenu_remove_item(Submenu* submenu, uint32_t index);

void submenu_reset(Submenu* submenu);

uint32_t submenu_get_selected_item(Submenu* submenu);

void submenu_set_selected_item(Submenu* submenu, uint32_t index);

void submenu_set_header(Submenu* submenu, const char* header);

void submenu_set_orientation(Submenu* submenu, ViewOrientation orientation);

#ifdef __cplusplus
}
#endif
