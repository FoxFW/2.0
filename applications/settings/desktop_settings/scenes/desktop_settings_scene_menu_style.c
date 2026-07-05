#include <applications.h>
#include <storage/storage.h>
#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

/* Direct file I/O for Fox theme — same reason as scene_start.c:
 * fox_theme_set/is_active are NOT in api_symbols.csv. */
#define FOX_THEME_FILE "/int/Fox.cfg"

static void fap_fox_theme_write(bool fox) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    if(storage_file_open(f, FOX_THEME_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t val = fox ? 1u : 0u;
        storage_file_write(f, &val, 1);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

#define MENU_STYLE_COUNT 2
static const char* const menu_style_text[MENU_STYLE_COUNT] = {"Classic", "Fox Theme"};

static void desktop_settings_scene_menu_style_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, menu_style_text[index]);
    app->settings.menu_theme = index;
}

void desktop_settings_scene_menu_style_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    VariableItemList* list = app->variable_item_list;
    VariableItem* item;

    item = variable_item_list_add(
        list, "Menu Style", MENU_STYLE_COUNT,
        desktop_settings_scene_menu_style_changed, app);
    uint8_t value_index =
        (app->settings.menu_theme < MENU_STYLE_COUNT) ? app->settings.menu_theme : 0;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, menu_style_text[value_index]);

    variable_item_list_add(list, "Exit to desktop to apply", 0, NULL, NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_menu_style_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        desktop_settings_save(&app->settings);
        fap_fox_theme_write(app->settings.menu_theme == 1);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    return false;
}

void desktop_settings_scene_menu_style_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    variable_item_list_reset(app->variable_item_list);
    desktop_settings_save(&app->settings);
    fap_fox_theme_write(app->settings.menu_theme == 1);
}