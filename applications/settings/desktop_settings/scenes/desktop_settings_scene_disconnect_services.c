#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <lib/toolbox/value_index.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"
#include <desktop/desktop_settings.h>

static VariableItem* s_ble_item   = NULL;
static VariableItem* s_gpio_item  = NULL;
static VariableItem* s_usb_item   = NULL;

static const char* const on_off_text[]   = {"OFF", "ON"};
static const char* const usb_level_text[] = {"Minimal", "CLI + RPC", "Full Disconnect"};

#define USB_LEVEL_COUNT 3

static void ds_update_children(DesktopSettingsApp* app) {
    bool enabled = (bool)app->settings.lock_on_lock_enabled;

    if(s_ble_item) {
        variable_item_set_current_value_text(
            s_ble_item,
            enabled ? on_off_text[app->settings.lock_disconnect_ble] : "N/A");
    }
    if(s_gpio_item) {
        variable_item_set_current_value_text(
            s_gpio_item,
            enabled ? on_off_text[app->settings.lock_disconnect_gpio] : "N/A");
    }
    if(s_usb_item) {
        variable_item_set_current_value_text(
            s_usb_item,
            enabled ? usb_level_text[app->settings.lock_usb_level] : "N/A");
    }
}

static void ds_push_settings(DesktopSettingsApp* app) {
    desktop_settings_save(&app->settings);
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(desktop, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void ds_on_lock_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_on_lock_enabled = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    ds_update_children(app);
    ds_push_settings(app);
}

static void ds_ble_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    if(!app->settings.lock_on_lock_enabled) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_disconnect_ble = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    ds_push_settings(app);
}

static void ds_gpio_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    if(!app->settings.lock_on_lock_enabled) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_disconnect_gpio = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    // TODO: call furi_hal_serial_control_disable/enable in desktop_lock/unlock
    //       once the correct ARF API function name is identified.
    ds_push_settings(app);
}

static void ds_usb_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    if(!app->settings.lock_on_lock_enabled) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_usb_level = index;
    variable_item_set_current_value_text(item, usb_level_text[index]);
    ds_push_settings(app);
}

static void ds_enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
    // No navigation items in this scene — all are variable items.
}

void desktop_settings_scene_disconnect_services_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    VariableItemList* var_list = app->variable_item_list;

    variable_item_list_reset(var_list);
    s_ble_item  = NULL;
    s_gpio_item = NULL;
    s_usb_item  = NULL;

    bool enabled = (bool)app->settings.lock_on_lock_enabled;

    VariableItem* on_lock_item =
        variable_item_list_add(var_list, "On Lock", 2, ds_on_lock_changed, app);
    variable_item_set_current_value_index(on_lock_item, app->settings.lock_on_lock_enabled);
    variable_item_set_current_value_text(on_lock_item, on_off_text[app->settings.lock_on_lock_enabled]);

    s_ble_item = variable_item_list_add(var_list, "BLE", 2, ds_ble_changed, app);
    variable_item_set_current_value_index(s_ble_item, enabled ? app->settings.lock_disconnect_ble : 0);
    variable_item_set_current_value_text(
        s_ble_item, enabled ? on_off_text[app->settings.lock_disconnect_ble] : "N/A");

    s_gpio_item = variable_item_list_add(var_list, "GPIO", 2, ds_gpio_changed, app);
    variable_item_set_current_value_index(s_gpio_item, enabled ? app->settings.lock_disconnect_gpio : 0);
    variable_item_set_current_value_text(
        s_gpio_item, enabled ? on_off_text[app->settings.lock_disconnect_gpio] : "N/A");

    s_usb_item = variable_item_list_add(var_list, "USB", USB_LEVEL_COUNT, ds_usb_changed, app);
    variable_item_set_current_value_index(s_usb_item, enabled ? app->settings.lock_usb_level : 0);
    variable_item_set_current_value_text(
        s_usb_item, enabled ? usb_level_text[app->settings.lock_usb_level] : "N/A");

    variable_item_list_set_enter_callback(var_list, ds_enter_callback, app);
    // Always start at the top — same shared-widget index issue as the parent
    // Security & Privacy menu. Without this the list opens wherever the index
    // happened to be left from whatever scene last used this widget.
    variable_item_list_set_selected_item(var_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_disconnect_services_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_disconnect_services_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    s_ble_item  = NULL;
    s_gpio_item = NULL;
    s_usb_item  = NULL;
    variable_item_list_reset(app->variable_item_list);
}
