#include <gui/modules/fox_theme.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

static void desktop_settings_scene_menu_style_changed(void* context, uint8_t style_index) {
    DesktopSettingsApp* app = context;
    app->settings.menu_theme = style_index;

    fox_theme_set_style(style_index);
}

void desktop_settings_scene_menu_style_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    uint8_t style_index = fox_theme_get_style();
    if(style_index > 4) style_index = 0;
    app->settings.menu_theme = style_index;

    desktop_settings_view_menu_style_set_callback(
        app->menu_style_view, desktop_settings_scene_menu_style_changed, app);
    desktop_settings_view_menu_style_set_selected(app->menu_style_view, style_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewMenuStyle);
}

bool desktop_settings_scene_menu_style_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_menu_style_on_exit(void* context) {
    UNUSED(context);
}
