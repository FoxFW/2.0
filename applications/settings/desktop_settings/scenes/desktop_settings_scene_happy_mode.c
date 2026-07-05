/* desktop_settings_scene_happy_mode.c
 *
 * Happy Mode is not accessible from the Fox Settings UI.
 * The original implementation called dolphin_get_settings() and
 * dolphin_set_settings() which are not available in the FAP API table.
 *
 * This file is intentionally stubbed with zero external dependencies so
 * the object file can be compiled without adding any missing imports to
 * the FAP's runtime import table — regardless of whether the linker's
 * --gc-sections prunes it as dead code or not.
 */

#include <gui/scene_manager.h>
#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

void desktop_settings_scene_happy_mode_on_enter(void* context) {
    /* Scene is unreachable — immediately return to previous scene. */
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