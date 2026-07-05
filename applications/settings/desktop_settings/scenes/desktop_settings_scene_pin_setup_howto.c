#include <stdint.h>
#include <core/check.h>
#include <gui/scene_manager.h>

#include "../desktop_settings_app.h"
#include <desktop/desktop_settings.h>
#include "../views/desktop_settings_view_pin_setup_howto.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"
#include "../desktop_settings_custom_event.h"

static void pin_setup_howto_done_callback(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DesktopSettingsCustomEventDone);
}

bool desktop_settings_scene_pin_setup_howto_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopSettingsCustomEventDone) {
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinSetup);
            consumed = true;
        }
    }

    return consumed;
}

void desktop_settings_scene_pin_setup_howto_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    desktop_settings_view_pin_setup_howto_set_callback(
        app->pin_setup_howto_view, pin_setup_howto_done_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPinSetupHowto);
}

void desktop_settings_scene_pin_setup_howto_on_exit(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    
    desktop_settings_view_pin_setup_howto_set_callback(app->pin_setup_howto_view, NULL, NULL);
}