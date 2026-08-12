#include <furi.h>
#include <desktop/desktop.h>
#include <desktop/desktop_settings.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

static void ds_alarm_edit_push_settings(DesktopSettingsApp* app) {
    desktop_settings_save(&app->settings);
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(desktop, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void desktop_settings_scene_alarm_edit_delete_callback(void* context) {
    DesktopSettingsApp* app = context;
    uint8_t index = (uint8_t)scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneAlarmEdit);

    if(index < app->settings.alarm_count) {
        for(uint8_t i = index; (uint8_t)(i + 1) < app->settings.alarm_count; i++) {
            app->settings.alarms[i] = app->settings.alarms[i + 1];
        }
        app->settings.alarm_count--;
        ds_alarm_edit_push_settings(app);
    }

    // Nothing left to save for this (now-deleted) alarm - skip straight back
    // to the list instead of going through the normal Back-save path.
    scene_manager_previous_scene(app->scene_manager);
}

void desktop_settings_scene_alarm_edit_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    uint8_t index = (uint8_t)scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneAlarmEdit);

    FoxAlarm alarm;
    if(index < app->settings.alarm_count) {
        alarm = app->settings.alarms[index];
    } else {
        alarm.hour = 8;
        alarm.minute = 0;
        alarm.days_mask = 0;
        alarm.active = 0;
        alarm.recurring = 0;
    }

    desktop_settings_view_alarm_edit_set_alarm(app->alarm_edit_view, &alarm);
    desktop_settings_view_alarm_edit_set_delete_callback(
        app->alarm_edit_view, desktop_settings_scene_alarm_edit_delete_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewAlarmEdit);
}

static void desktop_settings_scene_alarm_edit_save(DesktopSettingsApp* app) {
    uint8_t index = (uint8_t)scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneAlarmEdit);
    if(index >= app->settings.alarm_count) return;

    FoxAlarm alarm;
    desktop_settings_view_alarm_edit_get_alarm(app->alarm_edit_view, &alarm);
    app->settings.alarms[index] = alarm;
    ds_alarm_edit_push_settings(app);
}

bool desktop_settings_scene_alarm_edit_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        desktop_settings_scene_alarm_edit_save(app);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_alarm_edit_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    desktop_settings_view_alarm_edit_set_delete_callback(app->alarm_edit_view, NULL, NULL);
}
