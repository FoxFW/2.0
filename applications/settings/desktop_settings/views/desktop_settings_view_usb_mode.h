#pragma once
#include <gui/view.h>
#include <desktop/desktop.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DesktopSettingsViewUsbMode DesktopSettingsViewUsbMode;

typedef void (*DesktopSettingsViewUsbModeCallback)(void* context, DesktopUsbMode mode);

DesktopSettingsViewUsbMode* desktop_settings_view_usb_mode_alloc(void);
void desktop_settings_view_usb_mode_free(DesktopSettingsViewUsbMode* instance);
View* desktop_settings_view_usb_mode_get_view(DesktopSettingsViewUsbMode* instance);
void desktop_settings_view_usb_mode_set_callback(
    DesktopSettingsViewUsbMode* instance,
    DesktopSettingsViewUsbModeCallback callback,
    void* context);
void desktop_settings_view_usb_mode_set_desktop(
    DesktopSettingsViewUsbMode* instance,
    Desktop* desktop);
void desktop_settings_view_usb_mode_set_cursor(
    DesktopSettingsViewUsbMode* instance,
    DesktopUsbMode mode);

#ifdef __cplusplus
}
#endif
