#include <furi.h>
#include <gui/modules/submenu.h>
#include <locale/locale.h>
#include <desktop/desktop.h>
#include <desktop/desktop_settings.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

// Top toggles are fixed low indices; alarms start at ALARM_ITEM_BASE so the
// list can grow up to FOX_ALARM_MAX_COUNT without colliding with them.
#define ALARM_ITEM_KEEP_BACKLIGHT 0
#define ALARM_ITEM_BEEP           1
#define ALARM_ITEM_VIBRATE        2
#define ALARM_ITEM_BASE           10
#define ALARM_ITEM_ADD            0xFFFE

static void ds_alarm_push_settings(DesktopSettingsApp* app) {
    desktop_settings_save(&app->settings);
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(desktop, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void format_alarm_time(char* out, size_t out_size, uint8_t hour, uint8_t minute) {
    if(locale_get_time_format() == LocaleTimeFormat12h) {
        uint8_t h12 = hour % 12;
        if(h12 == 0) h12 = 12;
        snprintf(out, out_size, "%u:%02u%s", h12, minute, hour < 12 ? "AM" : "PM");
    } else {
        snprintf(out, out_size, "%02u:%02u", hour, minute);
    }
}

static void format_alarm_days(char* out, size_t out_size, const FoxAlarm* alarm) {
    if(!alarm->recurring) {
        strlcpy(out, "Once", out_size);
        return;
    }
    if((alarm->days_mask & 0x7F) == 0x7F) {
        strlcpy(out, "Daily", out_size);
        return;
    }
    if(alarm->days_mask == 0) {
        strlcpy(out, "Never", out_size);
        return;
    }
    static const uint8_t bit[7] = {
        FOX_ALARM_DAY_SUN,
        FOX_ALARM_DAY_MON,
        FOX_ALARM_DAY_TUE,
        FOX_ALARM_DAY_WED,
        FOX_ALARM_DAY_THU,
        FOX_ALARM_DAY_FRI,
        FOX_ALARM_DAY_SAT};
    static const char* const name[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    out[0] = '\0';
    bool first = true;
    for(uint8_t i = 0; i < 7; i++) {
        if(alarm->days_mask & bit[i]) {
            if(!first) strlcat(out, ",", out_size);
            strlcat(out, name[i], out_size);
            first = false;
        }
    }
}

static void desktop_settings_scene_alarm_clock_submenu_callback(void* context, uint32_t index) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void desktop_settings_scene_alarm_clock_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    char label[48];

    for(uint8_t i = 0; i < app->settings.alarm_count; i++) {
        const FoxAlarm* alarm = &app->settings.alarms[i];
        char time_str[16];
        char days_str[24];
        format_alarm_time(time_str, sizeof(time_str), alarm->hour, alarm->minute);
        format_alarm_days(days_str, sizeof(days_str), alarm);
        snprintf(
            label, sizeof(label), "[%c] %s  %s", alarm->active ? 'X' : ' ', time_str, days_str);
        submenu_add_item(
            submenu, label, ALARM_ITEM_BASE + i,
            desktop_settings_scene_alarm_clock_submenu_callback, app);
    }

    if(app->settings.alarm_count < FOX_ALARM_MAX_COUNT) {
        submenu_add_item(
            submenu, "+ Add Alarm", ALARM_ITEM_ADD,
            desktop_settings_scene_alarm_clock_submenu_callback, app);
    }

    snprintf(
        label, sizeof(label), "Keep Backlight On: %s",
        app->settings.alarm_keep_backlight_all_night ? "ON" : "OFF");
    submenu_add_item(
        submenu, label, ALARM_ITEM_KEEP_BACKLIGHT,
        desktop_settings_scene_alarm_clock_submenu_callback, app);

    snprintf(
        label, sizeof(label), "Alarm Beep: %s", app->settings.alarm_beep_enabled ? "ON" : "OFF");
    submenu_add_item(
        submenu, label, ALARM_ITEM_BEEP, desktop_settings_scene_alarm_clock_submenu_callback, app);

    snprintf(
        label, sizeof(label), "Alarm Vibrate: %s",
        app->settings.alarm_vibrate_enabled ? "ON" : "OFF");
    submenu_add_item(
        submenu, label, ALARM_ITEM_VIBRATE, desktop_settings_scene_alarm_clock_submenu_callback,
        app);

    submenu_set_header(submenu, "Fox Alarm Clock");

    uint32_t saved_pos =
        scene_manager_get_scene_state(app->scene_manager, DesktopSettingsAppSceneAlarmClock);
    submenu_set_selected_item(submenu, saved_pos);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewMenu);
}

bool desktop_settings_scene_alarm_clock_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, DesktopSettingsAppSceneAlarmClock, event.event);

        if(event.event == ALARM_ITEM_KEEP_BACKLIGHT) {
            app->settings.alarm_keep_backlight_all_night =
                !app->settings.alarm_keep_backlight_all_night;
            ds_alarm_push_settings(app);
            desktop_settings_scene_alarm_clock_on_enter(app);
            consumed = true;
        } else if(event.event == ALARM_ITEM_BEEP) {
            app->settings.alarm_beep_enabled = !app->settings.alarm_beep_enabled;
            ds_alarm_push_settings(app);
            desktop_settings_scene_alarm_clock_on_enter(app);
            consumed = true;
        } else if(event.event == ALARM_ITEM_VIBRATE) {
            app->settings.alarm_vibrate_enabled = !app->settings.alarm_vibrate_enabled;
            ds_alarm_push_settings(app);
            desktop_settings_scene_alarm_clock_on_enter(app);
            consumed = true;
        } else if(event.event == ALARM_ITEM_ADD) {
            if(app->settings.alarm_count < FOX_ALARM_MAX_COUNT) {
                FoxAlarm* alarm = &app->settings.alarms[app->settings.alarm_count];
                alarm->hour = 8;
                alarm->minute = 0;
                alarm->days_mask = 0;
                alarm->active = 0; // harmless until the user actually configures it
                alarm->recurring = 0;
                uint8_t new_index = app->settings.alarm_count;
                app->settings.alarm_count++;
                ds_alarm_push_settings(app);

                scene_manager_set_scene_state(
                    app->scene_manager, DesktopSettingsAppSceneAlarmEdit, new_index);
                scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneAlarmEdit);
            }
            consumed = true;
        } else if(event.event >= ALARM_ITEM_BASE && event.event < ALARM_ITEM_BASE + FOX_ALARM_MAX_COUNT) {
            uint8_t alarm_index = (uint8_t)(event.event - ALARM_ITEM_BASE);
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneAlarmEdit, alarm_index);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneAlarmEdit);
            consumed = true;
        }
    }

    return consumed;
}

void desktop_settings_scene_alarm_clock_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    submenu_reset(app->submenu);
}
