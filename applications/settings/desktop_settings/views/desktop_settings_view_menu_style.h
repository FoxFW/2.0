#pragma once
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DesktopSettingsViewMenuStyle DesktopSettingsViewMenuStyle;

typedef void (*DesktopSettingsViewMenuStyleCallback)(void* context, uint8_t style_index);

DesktopSettingsViewMenuStyle* desktop_settings_view_menu_style_alloc(void);
void desktop_settings_view_menu_style_free(DesktopSettingsViewMenuStyle* instance);
View* desktop_settings_view_menu_style_get_view(DesktopSettingsViewMenuStyle* instance);
void desktop_settings_view_menu_style_set_callback(
    DesktopSettingsViewMenuStyle* instance,
    DesktopSettingsViewMenuStyleCallback callback,
    void* context);
void desktop_settings_view_menu_style_set_selected(
    DesktopSettingsViewMenuStyle* instance,
    uint8_t style_index);

#ifdef __cplusplus
}
#endif
