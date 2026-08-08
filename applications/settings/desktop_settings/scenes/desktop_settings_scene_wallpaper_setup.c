#include <furi.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

void desktop_settings_scene_wallpaper_setup_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    desktop_settings_view_wallpaper_load(
        app->wallpaper_view, app->settings.wallpaper_filename, app->settings.wallpaper_enabled);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewWallpaper);
}

static void desktop_settings_scene_wallpaper_setup_save(DesktopSettingsApp* app) {
    bool enabled = false;
    char filename[sizeof(app->settings.wallpaper_filename)];
    desktop_settings_view_wallpaper_get(app->wallpaper_view, filename, sizeof(filename), &enabled);

    if(desktop_settings_view_wallpaper_has_files(app->wallpaper_view)) {
        strlcpy(app->settings.wallpaper_filename, filename, sizeof(app->settings.wallpaper_filename));
        app->settings.wallpaper_enabled = enabled ? 1 : 0;
    }
    // No valid files found - leave the previously saved selection untouched
    // rather than clobbering it with an empty filename.

    desktop_settings_save(&app->settings);
}

bool desktop_settings_scene_wallpaper_setup_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        desktop_settings_scene_wallpaper_setup_save(app);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_wallpaper_setup_on_exit(void* context) {
    UNUSED(context);
}
