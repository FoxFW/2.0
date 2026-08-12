#include <furi.h>
#include <gui/scene_manager.h>
#include "../desktop_i.h"
#include "desktop_scene.h"

enum {
    DesktopSceneClockLockEventExit,
};

static void desktop_scene_clock_lock_exit_callback(void* context) {
    Desktop* desktop = context;
    view_dispatcher_send_custom_event(desktop->view_dispatcher, DesktopSceneClockLockEventExit);
}

// Left = turn the backlight off right now, regardless of the "Keep
// Backlight On" setting. Right = turn it back on: if the setting is off this
// is just a normal on (it'll time out on its own like any button press); if
// the setting is on, this restores the persistent stay-on behavior. Neither
// key changes the saved setting itself - it's a momentary override for this
// viewing of the clock, reset the next time the screen is entered.
static void desktop_scene_clock_lock_backlight_callback(void* context, bool turn_on) {
    Desktop* desktop = context;
    if(turn_on) {
        desktop->clock_lock_backlight_manually_off = false;
        notification_message(
            desktop->notification,
            desktop->settings.alarm_keep_backlight_all_night ?
                &sequence_display_backlight_force_on :
                &sequence_display_backlight_on);
    } else {
        desktop->clock_lock_backlight_manually_off = true;
        notification_message(desktop->notification, &sequence_display_backlight_off);
    }
}

// Ticks once a second while this screen is showing - re-asserts the
// backlight so the normal auto-off timer never gets a chance to fire when
// "Keep Backlight On" is enabled. Skipped while the user has manually
// turned the backlight off via the Left-arrow shortcut, so that override
// actually sticks instead of being re-forced on a second later.
static void desktop_scene_clock_lock_tick_callback(void* context) {
    Desktop* desktop = context;
    if(desktop->settings.alarm_keep_backlight_all_night &&
       !desktop->clock_lock_backlight_manually_off) {
        notification_message(desktop->notification, &sequence_display_backlight_force_on);
    }
}

void desktop_scene_clock_lock_on_enter(void* context) {
    Desktop* desktop = context;
    desktop->on_clock_lock_scene = true;
    desktop->clock_lock_backlight_manually_off = false;

    // Listen for the exit trigger we wrote in the view
    desktop_clock_lock_set_callback(desktop->clock_lock_view, desktop_scene_clock_lock_exit_callback, desktop);
    desktop_clock_lock_set_backlight_callback(
        desktop->clock_lock_view, desktop_scene_clock_lock_backlight_callback, desktop);
    desktop_clock_lock_set_tick_callback(
        desktop->clock_lock_view, desktop_scene_clock_lock_tick_callback, desktop);

    if(desktop->settings.alarm_keep_backlight_all_night) {
        notification_message(desktop->notification, &sequence_display_backlight_force_on);
    }

    // Switch the screen to our clock (Fox Clock)
    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdClockLock);
}

bool desktop_scene_clock_lock_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopSceneClockLockEventExit) {
            // If an alarm was ringing, this same exit gesture (long-press
            // Down, or short OK while ringing) silences it first.
            if(desktop->alarm_ringing) {
                desktop_alarm_dismiss(desktop);
            }
            // Drops us back to the main desktop
            scene_manager_previous_scene(desktop->scene_manager);
            consumed = true;
        }
    }
    return consumed;
}

void desktop_scene_clock_lock_on_exit(void* context) {
    Desktop* desktop = context;
    desktop->on_clock_lock_scene = false;
    desktop->clock_lock_backlight_manually_off = false;
    desktop_clock_lock_set_callback(desktop->clock_lock_view, NULL, NULL);
    desktop_clock_lock_set_backlight_callback(desktop->clock_lock_view, NULL, NULL);
    desktop_clock_lock_set_tick_callback(desktop->clock_lock_view, NULL, NULL);
}