#include <furi.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/scene_manager.h>
#include <namechanger/namechanger.h>
#include <flipper_format/flipper_format.h>
#include <furi_hal_power.h>

#include <desktop/desktop.h>

#include "desktop_settings_app.h"
#include "scenes/desktop_settings_scene.h"
#include "views/desktop_settings_view_numeric_pin.h"

/* NOTE: <power/power_service/power.h> removed.
 * power_reboot() is replaced with furi_hal_power_reset() which is a direct
 * hardware reset from furi_hal — guaranteed to be in the API and does not
 * require the power service record to be open. */

static bool desktop_settings_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool desktop_settings_back_event_callback(void* context) {
    furi_assert(context);
    DesktopSettingsApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

DesktopSettingsApp* desktop_settings_app_alloc(void) {
    DesktopSettingsApp* app = malloc(sizeof(DesktopSettingsApp));

    desktop_settings_load(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&desktop_settings_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, desktop_settings_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, desktop_settings_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->popup = popup_alloc();
    app->submenu = submenu_alloc();
    app->variable_item_list = variable_item_list_alloc();

    app->pin_setup_howto_view = desktop_settings_view_pin_setup_howto_alloc();
    app->pin_setup_howto2_view = desktop_settings_view_pin_setup_howto2_alloc();
    app->numeric_pin_view = desktop_settings_view_numeric_pin_alloc();
    app->dialog_ex = dialog_ex_alloc();

    app->pin_menu_idx = DesktopSettingsAppViewIdPinInput;

    view_dispatcher_add_view(
        app->view_dispatcher, DesktopSettingsAppViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher,
        DesktopSettingsAppViewVarItemList,
        variable_item_list_get_view(app->variable_item_list));
    view_dispatcher_add_view(
        app->view_dispatcher, DesktopSettingsAppViewIdPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher,
        DesktopSettingsAppViewIdPinInput,
        desktop_settings_view_numeric_pin_get_view(app->numeric_pin_view));
    view_dispatcher_add_view(
        app->view_dispatcher,
        DesktopSettingsAppViewIdPinSetupHowto,
        desktop_settings_view_pin_setup_howto_get_view(app->pin_setup_howto_view));
    view_dispatcher_add_view(
        app->view_dispatcher,
        DesktopSettingsAppViewIdPinSetupHowto2,
        desktop_settings_view_pin_setup_howto2_get_view(app->pin_setup_howto2_view));
    view_dispatcher_add_view(
        app->view_dispatcher, DesktopSettingsAppViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        DesktopSettingsAppViewTextInput,
        text_input_get_view(app->text_input));

    return app;
}

void desktop_settings_app_free(DesktopSettingsApp* app) {
    furi_assert(app);

    bool temp_save_name = app->save_name;
    if(temp_save_name) {
        /* Write the name via the same pending-file mechanism that fox_setup uses.
         * desktop.c reads this on the next boot, writes NAMECHANGER_PATH correctly,
         * and reboots again to apply. Writing NAMECHANGER_PATH directly was unreliable. */
        Storage* storage = furi_record_open(RECORD_STORAGE);
        const char* pending = EXT_PATH("apps_data/fox_setup/name.pending");
        if(strcmp(app->device_name, "") == 0) {
            /* Empty name = restore default — write a single null byte as sentinel. */
            storage_simply_remove(storage, pending);
            /* Still need to clear NAMECHANGER_PATH directly for blank names. */
            storage_simply_remove(storage, NAMECHANGER_PATH);
        } else {
            storage_simply_mkdir(storage, EXT_PATH("apps_data/fox_setup"));
            File* f = storage_file_alloc(storage);
            if(storage_file_open(f, pending, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                storage_file_write(f, app->device_name, strlen(app->device_name));
                storage_file_close(f);
            }
            storage_file_free(f);
        }
        furi_record_close(RECORD_STORAGE);
    }

    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewIdPopup);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewIdPinInput);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewIdPinSetupHowto);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewIdPinSetupHowto2);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, DesktopSettingsAppViewTextInput);

    text_input_free(app->text_input);
    variable_item_list_free(app->variable_item_list);
    submenu_free(app->submenu);
    popup_free(app->popup);

    desktop_settings_view_pin_setup_howto_free(app->pin_setup_howto_view);
    desktop_settings_view_pin_setup_howto2_free(app->pin_setup_howto2_view);
    desktop_settings_view_numeric_pin_free(app->numeric_pin_view);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);

    /* Reboot the device after saving a new Flipper name.
     * Uses furi_hal_power_reset() instead of power_reboot() to avoid
     * depending on the power service API whose export status is unverified. */
    if(temp_save_name) {
        furi_hal_power_reset();
    }
}

int32_t desktop_settings_app(void* p) {
    UNUSED(p);

    DesktopSettingsApp* app = desktop_settings_app_alloc();
    scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    Desktop* desktop_svc = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(desktop_svc, &app->settings);
    furi_record_close(RECORD_DESKTOP);

    desktop_settings_app_free(app);

    return 0;
}
