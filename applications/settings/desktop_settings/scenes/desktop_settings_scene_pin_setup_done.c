#include <furi.h>
#include <assets_icons.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/popup.h>

#include "../desktop_settings_app.h"
#include "../desktop_settings_custom_event.h"
#include <desktop/desktop_settings.h>
#include "desktop_settings_scene.h"

static void pin_setup_done_popup_callback(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DesktopSettingsCustomEventDone);
}

void desktop_settings_scene_pin_setup_done_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    desktop_pin_code_set(&app->pincode_buffer);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_single_vibro);
    notification_message(notification, &sequence_blink_green_10);
    furi_record_close(RECORD_NOTIFICATION);

    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, pin_setup_done_popup_callback);
    popup_set_icon(app->popup, 0, 0, &I_fox_64x64);
    popup_set_header(app->popup, "Saved!", 96, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup,
        "Your PIN has\nbeen saved!",
        96,
        28,
        AlignCenter,
        AlignTop);    popup_set_timeout(app->popup, 1800);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);
}

bool desktop_settings_scene_pin_setup_done_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DesktopSettingsCustomEventDone: {
            bool scene_found = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DesktopSettingsAppScenePinMenu);
            if(!scene_found) {
                view_dispatcher_stop(app->view_dispatcher);
            }
            consumed = true;
            break;
        }
        default:
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
    }
    return consumed;
}

void desktop_settings_scene_pin_setup_done_on_exit(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    popup_reset(app->popup);
}
