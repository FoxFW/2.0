#include <furi.h>
#include <furi_hal.h>
#include <gui/scene_manager.h>
#include <gui/view_stack.h>
#include <stdint.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "../desktop.h"
#include "../desktop_i.h"
#include "../views/desktop_events.h"
#include "../views/desktop_view_pin_input.h"
#include "../helpers/pin_code.h"
#include "desktop_scene.h"

#define WRONG_PIN_HEADER_TIMEOUT 3000
#define INPUT_PIN_VIEW_TIMEOUT   15000
#define WIPER_SENTINEL        0xFF

typedef struct {
    FuriTimer* timer;
    FuriString* enter_pin_string;
} DesktopScenePinInputState;

static void desktop_scene_locked_light_red(bool value) {
    NotificationApp* app = furi_record_open(RECORD_NOTIFICATION);
    if(value) {
        notification_message(app, &sequence_set_only_red_255);
    } else {
        notification_message(app, &sequence_reset_red);
    }
    furi_record_close(RECORD_NOTIFICATION);
}

static void desktop_scene_pin_input_set_timer(Desktop* desktop, bool enable, uint32_t new_period) {
    furi_assert(desktop);

    DesktopScenePinInputState* state = (DesktopScenePinInputState*)scene_manager_get_scene_state(
        desktop->scene_manager, DesktopScenePinInput);
    furi_assert(state);
    if(enable) {
        furi_timer_start(state->timer, new_period);
    } else {
        furi_timer_stop(state->timer);
    }
}

static void desktop_scene_pin_input_back_callback(void* context) {
    Desktop* desktop = (Desktop*)context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopPinInputEventBack);
}

static void desktop_scene_pin_input_done_callback(const DesktopPinCode* pin_code, void* context) {
    Desktop* desktop = (Desktop*)context;

    if(desktop_pin_code_check(pin_code)) {
        FoxEscrowData escrow;
        memset(&escrow, 0, sizeof(FoxEscrowData));
        if(fox_escrow_load_and_verify(&escrow)) {
            // Always clear the fail count on correct PIN — the sentinel must
            // never lock out the legitimate owner.
            escrow.active_fail_count = 0;
            fox_escrow_save_state(&escrow);
        }
        view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopPinInputEventUnlocked);
    } else {
        FoxEscrowData escrow;
        memset(&escrow, 0, sizeof(FoxEscrowData));
        uint8_t fail_count;

        if(fox_escrow_load_and_verify(&escrow)) {
            if(escrow.active_fail_count == WIPER_SENTINEL) {
                desktop_view_pin_input_lock_input(desktop->pin_input_view);
                return;
            }
            escrow.active_fail_count++;
            fail_count = escrow.active_fail_count;
        } else {
            memset(&escrow, 0, sizeof(FoxEscrowData));
            escrow.active_fail_count = 1;
            escrow.wipe_limit = desktop->settings.pin_max_attempts;
            escrow.wipe_method = desktop->settings.pin_exceed_action;
            fail_count = 1;
        }
        fox_escrow_save_state(&escrow);
        fox_recovery_generate_file(fail_count);

        uint8_t limit = desktop->settings.pin_max_attempts;
        if(limit > 0 && fail_count >= limit) {
            // Trigger Wiper Screen: set sentinel, format SD, reboot.
            fox_escrow_trigger_wiper_screen();
            desktop_view_pin_input_lock_input(desktop->pin_input_view);
            fox_escrow_execute_wipe(); // formats SD card then calls furi_hal_power_reset()
        } else {
            view_dispatcher_send_custom_event(
                desktop->view_dispatcher, DesktopPinInputEventUnlockFailed);
        }
    }
}

static void desktop_scene_pin_input_timer_callback(void* context) {
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(
        desktop->view_dispatcher, DesktopPinInputEventResetWrongPinLabel);
}

static void desktop_scene_pin_input_update_wrong_count(DesktopScenePinInputState* state, Desktop* desktop) {
    UNUSED(state);
    FoxEscrowData escrow;
    uint8_t current_fails = 0;
    if(fox_escrow_load_and_verify(&escrow)) {
        current_fails = escrow.active_fail_count;
        if(current_fails == WIPER_SENTINEL) current_fails = 0;
    }
    desktop_view_pin_input_update_telemetry(
        desktop->pin_input_view, current_fails, desktop->settings.pin_max_attempts);
}

void desktop_scene_pin_input_on_enter(void* context) {
    Desktop* desktop = (Desktop*)context;

    desktop_view_pin_input_set_context(desktop->pin_input_view, desktop);
    desktop_view_pin_input_set_back_callback(
        desktop->pin_input_view, desktop_scene_pin_input_back_callback);
    desktop_view_pin_input_set_timeout_callback(
        desktop->pin_input_view, desktop_scene_pin_input_back_callback);
    desktop_view_pin_input_set_done_callback(
        desktop->pin_input_view, desktop_scene_pin_input_done_callback);

    DesktopScenePinInputState* state = malloc(sizeof(DesktopScenePinInputState));
    state->enter_pin_string = furi_string_alloc();
    state->timer =
        furi_timer_alloc(desktop_scene_pin_input_timer_callback, FuriTimerTypeOnce, desktop);
    scene_manager_set_scene_state(desktop->scene_manager, DesktopScenePinInput, (uint32_t)state);

    FoxEscrowData startup_check;
    memset(&startup_check, 0, sizeof(FoxEscrowData));
    if(fox_escrow_load_and_verify(&startup_check) &&
       startup_check.active_fail_count == WIPER_SENTINEL) {
        desktop_view_pin_input_lock_input(desktop->pin_input_view);

        // Full hardware lockout — cut every remote-access surface so the
        // device cannot be reached via qFlipper, BLE, CLI, or GPIO while
        // the wiper screen is showing.
        furi_hal_usb_set_config(NULL, NULL);  // Disconnect USB (kills qFlipper + USB-CDC CLI)
        // TODO: add BLE stop here using the same call Fox settings uses to disable BLE.
        // e.g. furi_hal_bt_stop_advertising() or the BT service record equivalent.
    } else {
        desktop_view_pin_input_unlock_input(desktop->pin_input_view);
        desktop_view_pin_input_hide_pin(desktop->pin_input_view, true);
        desktop_view_pin_input_set_label_button(desktop->pin_input_view, "OK");
        desktop_view_pin_input_set_label_primary(desktop->pin_input_view, 14, 25, "Enter PIN:");
        desktop_scene_pin_input_update_wrong_count(state, desktop);
        desktop_view_pin_input_reset_pin(desktop->pin_input_view);
    }

    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdPinInput);
}

bool desktop_scene_pin_input_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = (Desktop*)context;
    bool consumed = false;
    uint32_t pin_timeout = 0;
    DesktopScenePinInputState* state = (DesktopScenePinInputState*)scene_manager_get_scene_state(
        desktop->scene_manager, DesktopScenePinInput);

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case DesktopPinInputEventUnlockFailed:
            pin_timeout = desktop_pin_lock_get_fail_timeout();
            if(pin_timeout > 0) {
                desktop_pin_lock_error_notify();
                scene_manager_set_scene_state(
                    desktop->scene_manager, DesktopScenePinTimeout, pin_timeout);
                scene_manager_next_scene(desktop->scene_manager, DesktopScenePinTimeout);
            } else {
                desktop_pin_lock_error_notify();
                desktop_scene_locked_light_red(true);

                FoxEscrowData check;
                uint8_t cur_fails = 0;
                uint8_t limit = desktop->settings.pin_max_attempts;
                if(fox_escrow_load_and_verify(&check)) cur_fails = check.active_fail_count;
                bool final_warning = (limit > 0 && cur_fails > 0 && (cur_fails + 1) >= limit);

                // Brief overlay — view's internal timer clears it and resets digit entry
                desktop_view_pin_input_show_wrong_pin_feedback(
                    desktop->pin_input_view,
                    final_warning ? "WIPE ON NEXT!" : "Incorrect PIN",
                    WRONG_PIN_HEADER_TIMEOUT);

                // Red light stays on until ResetWrongPinLabel fires
                if(!final_warning) {
                    desktop_scene_pin_input_set_timer(desktop, true, WRONG_PIN_HEADER_TIMEOUT);
                }

                desktop_scene_pin_input_update_wrong_count(state, desktop);
                desktop_view_pin_input_reset_pin(desktop->pin_input_view);
            }
            consumed = true;
            break;
        case DesktopPinInputEventResetWrongPinLabel:
            desktop_scene_locked_light_red(false);
            desktop_scene_pin_input_update_wrong_count(state, desktop);
            consumed = true;
            break;
        case DesktopPinInputEventUnlocked:
        case DesktopGlobalApiUnlock:
            desktop_unlock(desktop);
            consumed = true;
            break;
        case DesktopPinInputEventBack:
            scene_manager_search_and_switch_to_previous_scene(
                desktop->scene_manager, DesktopSceneLocked);
            notification_message(desktop->notification, &sequence_display_backlight_off);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void desktop_scene_pin_input_on_exit(void* context) {
    Desktop* desktop = (Desktop*)context;
    desktop_scene_locked_light_red(false);

    DesktopScenePinInputState* state = (DesktopScenePinInputState*)scene_manager_get_scene_state(
        desktop->scene_manager, DesktopScenePinInput);

    furi_timer_free(state->timer);
    furi_string_free(state->enter_pin_string);
    free(state);
}