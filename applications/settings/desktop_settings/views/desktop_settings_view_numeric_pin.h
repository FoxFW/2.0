#pragma once

#include <gui/view.h>
#include <stdint.h>

typedef struct DesktopSettingsViewNumericPin DesktopSettingsViewNumericPin;

typedef void (*DesktopSettingsViewNumericPinCallback)(bool success, void* context);

DesktopSettingsViewNumericPin* desktop_settings_view_numeric_pin_alloc(void);

void desktop_settings_view_numeric_pin_free(DesktopSettingsViewNumericPin* instance);

View* desktop_settings_view_numeric_pin_get_view(DesktopSettingsViewNumericPin* instance);

void desktop_settings_view_numeric_pin_set_callback(
    DesktopSettingsViewNumericPin* instance,
    DesktopSettingsViewNumericPinCallback callback,
    void* context);

void desktop_settings_view_numeric_pin_reset(DesktopSettingsViewNumericPin* instance);

void desktop_settings_view_numeric_pin_set_mode(DesktopSettingsViewNumericPin* instance, bool is_confirm);

void desktop_settings_view_numeric_pin_set_error(DesktopSettingsViewNumericPin* instance, bool error);

void desktop_settings_view_numeric_pin_get_pin(DesktopSettingsViewNumericPin* instance, uint8_t* pin_buffer, uint8_t* pin_length);