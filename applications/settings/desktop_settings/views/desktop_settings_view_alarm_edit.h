#pragma once

#include <gui/view.h>
#include <desktop/desktop_settings.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DesktopSettingsViewAlarmEdit DesktopSettingsViewAlarmEdit;

// Fired on a long OK press while the "Delete Alarm" field is selected -
// deliberately harder to trigger by accident than a short press, since
// there's no separate confirm screen for this editor.
typedef void (*DesktopSettingsAlarmEditDeleteCallback)(void* context);

DesktopSettingsViewAlarmEdit* desktop_settings_view_alarm_edit_alloc(void);
void desktop_settings_view_alarm_edit_free(DesktopSettingsViewAlarmEdit* instance);
View* desktop_settings_view_alarm_edit_get_view(DesktopSettingsViewAlarmEdit* instance);

// Loads an alarm into the editor's working copy and resets the field cursor
// to the first field (Hour).
void desktop_settings_view_alarm_edit_set_alarm(
    DesktopSettingsViewAlarmEdit* instance,
    const FoxAlarm* alarm);

// Reads the editor's current working copy back out - call this whenever you
// need the latest edited values (e.g. on scene exit, to persist).
void desktop_settings_view_alarm_edit_get_alarm(
    DesktopSettingsViewAlarmEdit* instance,
    FoxAlarm* out);

void desktop_settings_view_alarm_edit_set_delete_callback(
    DesktopSettingsViewAlarmEdit* instance,
    DesktopSettingsAlarmEditDeleteCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
