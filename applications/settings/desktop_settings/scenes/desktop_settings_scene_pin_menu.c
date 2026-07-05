#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <applications.h>
#include <lib/toolbox/value_index.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"
#include "../desktop_settings_custom_event.h"
#include <desktop/desktop_settings.h>

static uint8_t s_pin_action_count = 0;
static VariableItem* s_exceed_item = NULL;

#define AUTO_LOCK_DELAY_COUNT 9
static const char* const auto_lock_delay_text[AUTO_LOCK_DELAY_COUNT] = {
    "OFF", "10s", "15s", "30s", "60s", "90s", "2min", "5min", "10min",
};
static const uint32_t auto_lock_delay_value[AUTO_LOCK_DELAY_COUNT] =
    {0, 10000, 15000, 30000, 60000, 90000, 120000, 300000, 600000};

#define USB_INHIBIT_COUNT 2
static const char* const usb_inhibit_text[USB_INHIBIT_COUNT] = {"OFF", "ON"};

static const char* const s_max_attempts_labels[] = {
    "No Limit", "3", "4", "5", "6", "7", "8", "9", "10"
};

static const char* const s_exceed_labels[] = {
    "Lock only", "Format SD"
};

static uint8_t index_to_max_attempts(uint8_t idx) {
    return (idx == 0) ? 0 : (uint8_t)(idx + 2);
}

static uint8_t max_attempts_to_index(uint8_t val) {
    if(val < 3) return 0;
    if(val > 10) return 8;
    return (uint8_t)(val - 2);
}

static void pin_menu_max_attempts_change(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.pin_max_attempts = index_to_max_attempts(idx);
    variable_item_set_current_value_text(item, s_max_attempts_labels[idx]);
    desktop_settings_save(&app->settings);
    Desktop* _d = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(_d, &app->settings);
    furi_record_close(RECORD_DESKTOP);

    if(s_exceed_item != NULL) {
        if(app->settings.pin_max_attempts == 0) {
            variable_item_set_current_value_text(s_exceed_item, "N/A");
        } else {
            variable_item_set_current_value_text(
                s_exceed_item, s_exceed_labels[app->settings.pin_exceed_action]);
        }
    }
}

static void pin_menu_exceed_action_change(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    if(app->settings.pin_max_attempts == 0) {
        variable_item_set_current_value_index(item, app->settings.pin_exceed_action);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.pin_exceed_action = idx;
    variable_item_set_current_value_text(item, s_exceed_labels[idx]);
    desktop_settings_save(&app->settings);
    Desktop* _d = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(_d, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void pin_menu_auto_lock_delay_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, auto_lock_delay_text[index]);
    app->settings.auto_lock_delay_ms = auto_lock_delay_value[index];
    desktop_settings_save(&app->settings);
    Desktop* _d = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(_d, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void pin_menu_usb_inhibit_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, usb_inhibit_text[index]);
    app->settings.usb_inhibit_auto_lock = index;
    desktop_settings_save(&app->settings);
    Desktop* _d = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(_d, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void pin_menu_enter_callback(void* context, uint32_t index) {
    DesktopSettingsApp* app = context;

    if(index < (uint32_t)s_pin_action_count) {
        // PIN set / change / remove buttons
        if(!desktop_pin_code_is_set()) {
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinSetupHowto);
        } else if(index == 0) {
            scene_manager_set_scene_state(
                app->scene_manager,
                DesktopSettingsAppScenePinAuth,
                SCENE_STATE_PIN_AUTH_CHANGE_PIN);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinAuth);
        } else if(index == 1) {
            scene_manager_set_scene_state(
                app->scene_manager,
                DesktopSettingsAppScenePinAuth,
                SCENE_STATE_PIN_AUTH_DISABLE);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinAuth);
        }
    } else if(index == (uint32_t)(s_pin_action_count + 2)) {
        // "Advanced Security" navigation button (index = PIN buttons + MAX Attempts + On Exceed)
        scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneDisconnectServices);
    }
    // All other indices are variable items handled by their own change callbacks
}

void desktop_settings_scene_pin_menu_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    VariableItemList* var_list = app->variable_item_list;

    variable_item_list_reset(var_list);
    s_exceed_item = NULL;

    if(!desktop_pin_code_is_set()) {
        s_pin_action_count = 1;
        variable_item_list_add(var_list, "Set PIN", 0, NULL, NULL);
    } else {
        s_pin_action_count = 2;
        variable_item_list_add(var_list, "Change PIN", 0, NULL, NULL);
        variable_item_list_add(var_list, "Remove PIN", 0, NULL, NULL);
    }

    VariableItem* attempts_item = variable_item_list_add(
        var_list, "MAX Attempts", 9, pin_menu_max_attempts_change, app);
    uint8_t attempts_idx = max_attempts_to_index(app->settings.pin_max_attempts);
    variable_item_set_current_value_index(attempts_item, attempts_idx);
    variable_item_set_current_value_text(attempts_item, s_max_attempts_labels[attempts_idx]);

    s_exceed_item = variable_item_list_add(
        var_list, "On Exceed", 2, pin_menu_exceed_action_change, app);
    variable_item_set_current_value_index(s_exceed_item, app->settings.pin_exceed_action);
    variable_item_set_current_value_text(
        s_exceed_item,
        app->settings.pin_max_attempts == 0 ? "N/A" : s_exceed_labels[app->settings.pin_exceed_action]);

    // Navigation button at index s_pin_action_count + 2 — handled in enter_callback
    variable_item_list_add(var_list, "Advanced Security", 0, NULL, NULL);

    VariableItem* auto_lock_item = variable_item_list_add(
        var_list, "Auto Lock Timer", AUTO_LOCK_DELAY_COUNT, pin_menu_auto_lock_delay_changed, app);
    uint8_t auto_lock_idx = value_index_uint32(
        app->settings.auto_lock_delay_ms, auto_lock_delay_value, AUTO_LOCK_DELAY_COUNT);
    variable_item_set_current_value_index(auto_lock_item, auto_lock_idx);
    variable_item_set_current_value_text(auto_lock_item, auto_lock_delay_text[auto_lock_idx]);

    VariableItem* usb_item = variable_item_list_add(
        var_list, "USB: No AutoLock", USB_INHIBIT_COUNT, pin_menu_usb_inhibit_changed, app);
    variable_item_set_current_value_index(usb_item, app->settings.usb_inhibit_auto_lock);
    variable_item_set_current_value_text(
        usb_item, usb_inhibit_text[app->settings.usb_inhibit_auto_lock]);

    variable_item_list_set_enter_callback(var_list, pin_menu_enter_callback, app);
    // Always start at the top — the shared VariableItemList widget otherwise keeps
    // whatever index was last selected in a DIFFERENT scene (e.g. Disconnect
    // Services), so without this the menu can appear scrolled to a random row.
    variable_item_list_set_selected_item(var_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_pin_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_pin_menu_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    s_exceed_item = NULL;
    variable_item_list_reset(app->variable_item_list);
}