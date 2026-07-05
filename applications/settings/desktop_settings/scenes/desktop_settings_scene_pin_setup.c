#include "../desktop_settings_app.h"
#include "../views/desktop_settings_view_numeric_pin.h"
#include "desktop_settings_scene.h"
#include <furi.h>
#include <desktop/helpers/pin_code.h>

/* NOTE: notification/notification_messages.h removed.
 * notification_message, sequence_single_vibro are notification-service symbols
 * whose SDK export status we cannot verify without the service's application.fam.
 * Vibration on PIN mismatch is replaced with a no-op; the error display via
 * desktop_settings_view_numeric_pin_set_error() still gives visual feedback. */

static uint8_t first_pass_digits[8];
static uint8_t first_pass_len = 0;
static uint8_t fail_attempts = 0;
static bool confirmation_phase_active = false;

static const uint8_t pin_encode_k1[10] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
static const uint8_t pin_encode_k2[10] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1};

enum {
    LocalSceneEventPinSubmitted,
    LocalSceneEventBackTriggered,
};

void pin_setup_numeric_callback(bool success, void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    if(success) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LocalSceneEventPinSubmitted);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, LocalSceneEventBackTriggered);
    }
}

void desktop_settings_scene_pin_setup_on_enter(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;

    confirmation_phase_active = false;
    first_pass_len = 0;

    desktop_settings_view_numeric_pin_reset(app->numeric_pin_view);
    desktop_settings_view_numeric_pin_set_mode(app->numeric_pin_view, false);
    desktop_settings_view_numeric_pin_set_callback(
        app->numeric_pin_view, pin_setup_numeric_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewIdPinInput);
}

bool desktop_settings_scene_pin_setup_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LocalSceneEventPinSubmitted) {
            if(!confirmation_phase_active) {
                desktop_settings_view_numeric_pin_get_pin(
                    app->numeric_pin_view, first_pass_digits, &first_pass_len);
                confirmation_phase_active = true;
                desktop_settings_view_numeric_pin_reset(app->numeric_pin_view);
                desktop_settings_view_numeric_pin_set_mode(app->numeric_pin_view, true);
                consumed = true;
            } else {
                uint8_t confirmation_digits[8];
                uint8_t confirmation_len = 0;
                desktop_settings_view_numeric_pin_get_pin(
                    app->numeric_pin_view, confirmation_digits, &confirmation_len);

                if(first_pass_len == confirmation_len &&
                   memcmp(first_pass_digits, confirmation_digits, first_pass_len) == 0) {
                    memset(&app->pincode_buffer, 0, sizeof(DesktopPinCode));
                    app->pincode_buffer.length = 0;
                    for(uint8_t i = 0; i < first_pass_len; i++) {
                        uint8_t d = first_pass_digits[i] % 10;
                        app->pincode_buffer.data[app->pincode_buffer.length++] =
                            (char)pin_encode_k1[d];
                        app->pincode_buffer.data[app->pincode_buffer.length++] =
                            (char)pin_encode_k2[d];
                    }

                    Desktop* desktop_service = furi_record_open(RECORD_DESKTOP);
                    desktop_set_pin(desktop_service, &app->pincode_buffer);
                    furi_record_close(RECORD_DESKTOP);

                    fail_attempts = 0;
                    scene_manager_next_scene(
                        app->scene_manager, DesktopSettingsAppScenePinSetupDone);
                } else {
                    /* PIN mismatch — show error state in the numeric pin view.
                     * Notification vibration removed (uncertain API export). */
                    desktop_settings_view_numeric_pin_set_error(app->numeric_pin_view, true);
                }
                consumed = true;
            }
        } else if(event.event == LocalSceneEventBackTriggered) {
            if(confirmation_phase_active) {
                confirmation_phase_active = false;
                desktop_settings_view_numeric_pin_reset(app->numeric_pin_view);
                desktop_settings_view_numeric_pin_set_mode(app->numeric_pin_view, false);
                consumed = true;
            } else {
                consumed = scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, DesktopSettingsAppSceneStart);
            }
        }
    }
    return consumed;
}

void desktop_settings_scene_pin_setup_on_exit(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    desktop_settings_view_numeric_pin_set_callback(app->numeric_pin_view, NULL, NULL);
}