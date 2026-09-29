#pragma once

#include <gui/view.h>
#include <desktop/desktop_settings.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DesktopSettingsViewAlarmEdit DesktopSettingsViewAlarmEdit;

typedef void (*DesktopSettingsAlarmEditDeleteCallback)(void* context);

DesktopSettingsViewAlarmEdit* desktop_settings_view_alarm_edit_alloc(void);
void desktop_settings_view_alarm_edit_free(DesktopSettingsViewAlarmEdit* instance);
View* desktop_settings_view_alarm_edit_get_view(DesktopSettingsViewAlarmEdit* instance);

void desktop_settings_view_alarm_edit_set_alarm(
    DesktopSettingsViewAlarmEdit* instance,
    const FoxAlarm* alarm);

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
