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

#define CLOCK_LOCK_BRIGHTNESS_STEP (0.05f)

static void desktop_scene_clock_lock_brightness_callback(void* context, bool increase) {
    Desktop* desktop = context;
    NotificationApp* notification = desktop->notification;

    float brightness = notification->settings.display_brightness;
    brightness += increase ? CLOCK_LOCK_BRIGHTNESS_STEP : -CLOCK_LOCK_BRIGHTNESS_STEP;
    if(brightness > 1.0f) brightness = 1.0f;
    if(brightness < 0.0f) brightness = 0.0f;
    notification->settings.display_brightness = brightness;

    notification_message(notification, &sequence_display_backlight_force_on);
    notification_message_save_settings(notification);

    desktop_clock_lock_show_brightness(
        desktop->clock_lock_view, (uint8_t)(brightness * 100.0f + 0.5f));
}

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

    desktop_clock_lock_set_callback(desktop->clock_lock_view, desktop_scene_clock_lock_exit_callback, desktop);
    desktop_clock_lock_set_backlight_callback(
        desktop->clock_lock_view, desktop_scene_clock_lock_backlight_callback, desktop);
    desktop_clock_lock_set_brightness_callback(
        desktop->clock_lock_view, desktop_scene_clock_lock_brightness_callback, desktop);
    desktop_clock_lock_set_tick_callback(
        desktop->clock_lock_view, desktop_scene_clock_lock_tick_callback, desktop);

    if(desktop->settings.alarm_keep_backlight_all_night) {
        notification_message(desktop->notification, &sequence_display_backlight_force_on);
    }

    view_dispatcher_switch_to_view(desktop->view_dispatcher, DesktopViewIdClockLock);
}

bool desktop_scene_clock_lock_on_event(void* context, SceneManagerEvent event) {
    Desktop* desktop = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DesktopSceneClockLockEventExit) {

            if(desktop->alarm_ringing) {
                desktop_alarm_dismiss(desktop);
            }

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
    desktop_clock_lock_set_brightness_callback(desktop->clock_lock_view, NULL, NULL);
    desktop_clock_lock_set_tick_callback(desktop->clock_lock_view, NULL, NULL);
}
