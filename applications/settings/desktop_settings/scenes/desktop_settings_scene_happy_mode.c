#include <gui/scene_manager.h>
#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

void desktop_settings_scene_happy_mode_on_enter(void* context) {

    DesktopSettingsApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

bool desktop_settings_scene_happy_mode_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_happy_mode_on_exit(void* context) {
    UNUSED(context);
}
