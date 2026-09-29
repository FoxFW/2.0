#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include <desktop/desktop.h>
#include <loader/loader.h>

#define MASS_STORAGE_FAP_PATH EXT_PATH("apps/USB/mass_storage.fap")
#define MASS_STORAGE_SD_CARD_ARG "sdcard"

static void desktop_settings_scene_usb_mode_chosen(void* context, DesktopUsbMode mode) {
    DesktopSettingsApp* app = context;
    if(mode != DesktopUsbModeMassStorage) return;

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(
        loader, MASS_STORAGE_FAP_PATH, MASS_STORAGE_SD_CARD_ARG, LoaderDeferredLaunchFlagNone);
    furi_record_close(RECORD_LOADER);

    scene_manager_stop(app->scene_manager);
    view_dispatcher_stop(app->view_dispatcher);
}

void desktop_settings_scene_usb_mode_on_enter(void* context) {
    DesktopSettingsApp* app = context;

    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_settings_view_usb_mode_set_desktop(app->usb_mode_view, desktop);
    desktop_settings_view_usb_mode_set_cursor(
        app->usb_mode_view, desktop_api_get_usb_mode(desktop));
    desktop_settings_view_usb_mode_set_callback(
        app->usb_mode_view, desktop_settings_scene_usb_mode_chosen, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewUsbMode);
}

bool desktop_settings_scene_usb_mode_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_usb_mode_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    desktop_settings_view_usb_mode_set_desktop(app->usb_mode_view, NULL);
    furi_record_close(RECORD_DESKTOP);
}
