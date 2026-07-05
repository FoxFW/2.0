// desktop_i.h was previously included here but nothing from it is actually used
// in this scene — it was vestigial and breaks external FAP compilation.
#include <desktop/helpers/pin_code.h>   // SDK-style path (was absolute applications/services/...)
#include "desktop_settings_scene.h"
#include <assets_icons.h>

#include "../desktop_settings_app.h"
#include "../desktop_settings_custom_event.h"

static void desktop_settings_scene_pin_error_popup_callback(void* context) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DesktopSettingsCustomEventExit);
}

void desktop_settings_scene_pin_error_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    popup_reset(app->popup);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, desktop_settings_scene_pin_error_popup_callback);
    popup_set_header(app->popup, "WRONG!", 96, 5, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Access Denied.\nToo many fails\ntriggers lock.", 96, 22, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 8, 18, &I_DolphinCommon_56x48);
    popup_set_timeout(app->popup, 2000);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);
}

bool desktop_settings_scene_pin_error_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopSettingsCustomEventExit) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_pin_error_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    popup_reset(app->popup);
}