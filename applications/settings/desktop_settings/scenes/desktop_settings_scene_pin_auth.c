#include <stdint.h>
#include <core/check.h>
#include <gui/scene_manager.h>
#include <desktop/helpers/pin_code.h>
#include "../desktop_settings_app.h"
#include "../desktop_settings_custom_event.h"
#include <desktop/desktop_settings.h>

// FIXED: Removed the unexported kernel view header
#include "../views/desktop_settings_view_numeric_pin.h" 
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

// 2-byte encoding matching desktop_view_pin_input_rebuild_pin in the lock screen.
// Each digit maps to two bytes (k1, k2) — must stay in sync with the lock screen.
static const uint8_t pin_auth_k1[10] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
static const uint8_t pin_auth_k2[10] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1};

static void pin_auth_numeric_callback(bool success, void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;

    if(success) {
        uint8_t digits[8];
        uint8_t digit_count = 0;
        desktop_settings_view_numeric_pin_get_pin(app->numeric_pin_view, digits, &digit_count);

        // Encode digits → 2-byte pairs, matching the lock screen encoding exactly
        memset(&app->pincode_buffer, 0, sizeof(DesktopPinCode));
        app->pincode_buffer.length = 0;
        for(uint8_t i = 0; i < digit_count; i++) {
            uint8_t d = digits[i] % 10;
            app->pincode_buffer.data[app->pincode_buffer.length++] = (char)pin_auth_k1[d];
            app->pincode_buffer.data[app->pincode_buffer.length++] = (char)pin_auth_k2[d];
        }

        // Check if the input pin matches the system pin
        if(desktop_pin_code_check(&app->pincode_buffer)) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, DesktopSettingsCustomEventPinsEqual);
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, DesktopSettingsCustomEventPinsDifferent);
        }
    } else {
        // Handle back/exit selection from the keyboard view matrix
        view_dispatcher_send_custom_event(app->view_dispatcher, DesktopSettingsCustomEventExit);
    }
}

void desktop_settings_scene_pin_auth_on_enter(void* context) {
    furi_assert(desktop_pin_code_is_set());
    DesktopSettingsApp* app = context;

    // FIXED: Point authentication execution flows strictly to our custom local view modules
    desktop_settings_view_numeric_pin_reset(app->numeric_pin_view);
    desktop_settings_view_numeric_pin_set_callback(
        app->numeric_pin_view, pin_auth_numeric_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPinInput);
}

bool desktop_settings_scene_pin_auth_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DesktopSettingsCustomEventPinsDifferent:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppScenePinError, SCENE_STATE_PIN_ERROR_WRONG);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinError);
            consumed = true;
            break;
        case DesktopSettingsCustomEventPinsEqual: {
            uint32_t state =
                scene_manager_get_scene_state(app->scene_manager, DesktopSettingsAppScenePinAuth);
            if(state == SCENE_STATE_PIN_AUTH_CHANGE_PIN) {
                scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinSetupHowto);
            } else if(state == SCENE_STATE_PIN_AUTH_DISABLE) {
                scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinDisable);
            } else {
                furi_crash();
            }
            consumed = true;
            break;
        }
        case DesktopSettingsCustomEventExit:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DesktopSettingsAppScenePinMenu);
            consumed = true;
            break;

        default:
            consumed = true;
            break;
        }
    }
    return consumed;
}

void desktop_settings_scene_pin_auth_on_exit(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    // FIXED: Safely clean out the local numeric module references
    desktop_settings_view_numeric_pin_set_callback(app->numeric_pin_view, NULL, NULL);
}