#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <lib/toolbox/value_index.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"
#include <desktop/desktop_settings.h>

static VariableItem* s_seconds_item = NULL;

static const char* const on_off_text[] = {"OFF", "ON"};

static void ld_push_settings(DesktopSettingsApp* app) {
    desktop_settings_save(&app->settings);
    Desktop* desktop = furi_record_open(RECORD_DESKTOP);
    desktop_api_set_settings(desktop, &app->settings);
    furi_record_close(RECORD_DESKTOP);
}

static void ld_show_time_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_show_time = index;
    variable_item_set_current_value_text(item, on_off_text[index]);

    if(s_seconds_item) {
        variable_item_set_current_value_text(
            s_seconds_item, index ? on_off_text[app->settings.lock_show_seconds] : "N/A");
    }
    ld_push_settings(app);
}

static void ld_show_seconds_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    if(!app->settings.lock_show_time) {
        variable_item_set_current_value_index(item, app->settings.lock_show_seconds);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_show_seconds = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    ld_push_settings(app);
}

static void ld_show_date_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_show_date = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    ld_push_settings(app);
}

static void ld_show_statusbar_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_show_statusbar = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    ld_push_settings(app);
}

static void ld_unlock_prompt_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.lock_unlock_prompt = index;
    variable_item_set_current_value_text(item, on_off_text[index]);
    ld_push_settings(app);
}

static void ld_enter_callback(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
    // No navigation items in this scene — all are variable items.
}

void desktop_settings_scene_lock_display_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    VariableItemList* var_list = app->variable_item_list;

    variable_item_list_reset(var_list);
    s_seconds_item = NULL;

    VariableItem* time_item =
        variable_item_list_add(var_list, "Show Time", 2, ld_show_time_changed, app);
    variable_item_set_current_value_index(time_item, app->settings.lock_show_time);
    variable_item_set_current_value_text(time_item, on_off_text[app->settings.lock_show_time]);

    s_seconds_item =
        variable_item_list_add(var_list, "Show Seconds", 2, ld_show_seconds_changed, app);
    variable_item_set_current_value_index(
        s_seconds_item, app->settings.lock_show_time ? app->settings.lock_show_seconds : 0);
    variable_item_set_current_value_text(
        s_seconds_item,
        app->settings.lock_show_time ? on_off_text[app->settings.lock_show_seconds] : "N/A");

    VariableItem* date_item =
        variable_item_list_add(var_list, "Show Date", 2, ld_show_date_changed, app);
    variable_item_set_current_value_index(date_item, app->settings.lock_show_date);
    variable_item_set_current_value_text(date_item, on_off_text[app->settings.lock_show_date]);

    VariableItem* statusbar_item =
        variable_item_list_add(var_list, "Show Statusbar", 2, ld_show_statusbar_changed, app);
    variable_item_set_current_value_index(statusbar_item, app->settings.lock_show_statusbar);
    variable_item_set_current_value_text(
        statusbar_item, on_off_text[app->settings.lock_show_statusbar]);

    VariableItem* prompt_item =
        variable_item_list_add(var_list, "Unlock Prompt", 2, ld_unlock_prompt_changed, app);
    variable_item_set_current_value_index(prompt_item, app->settings.lock_unlock_prompt);
    variable_item_set_current_value_text(
        prompt_item, on_off_text[app->settings.lock_unlock_prompt]);

    variable_item_list_set_enter_callback(var_list, ld_enter_callback, app);
    variable_item_list_set_selected_item(var_list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_lock_display_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_lock_display_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    s_seconds_item = NULL;
    variable_item_list_reset(app->variable_item_list);
}
