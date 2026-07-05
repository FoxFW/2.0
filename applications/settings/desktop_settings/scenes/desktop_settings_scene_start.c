#include <applications.h>
#include <lib/toolbox/value_index.h>
#include <gui/modules/fox_theme.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"
#include "desktop_settings_scene_i.h"

/* fox_theme_set() and fox_theme_is_active() are confirmed present in
 * api_symbols.csv with '+'. The previous Missing Imports was caused solely
 * by I_fox_64x64 / I_DolphinCommon_56x48 icon variables (now fixed via
 * fap_icon_assets). These API calls are safe to use directly. */

typedef enum {
    DesktopSettingsPinSetup           = 0,
    DesktopSettingsWallpaper          = 1,
    DesktopSettingsMenuStyle          = 2,
    DesktopSettingsChangeName         = 3,
    DesktopSettingsFavoriteLeftShort  = 6,
    DesktopSettingsFavoriteLeftLong   = 7,
    DesktopSettingsFavoriteRightShort = 8,
    DesktopSettingsFavoriteRightLong  = 9,
    DesktopSettingsFavoriteOkLong     = 10,
} DesktopSettingsEntry;

#define CLOCK_ENABLE_COUNT 2
static const char* const clock_enable_text[CLOCK_ENABLE_COUNT]  = {"OFF", "ON"};
static const uint32_t    clock_enable_value[CLOCK_ENABLE_COUNT] = {0, 1};

#define MENU_STYLE_COUNT 2
static const char* const menu_style_text[MENU_STYLE_COUNT] = {"Classic", "Default"};

static void desktop_settings_scene_start_menu_style_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, menu_style_text[index]);
    app->settings.menu_theme = index;
    /* Update g_fox_theme immediately so all GUI modules (submenu borders,
     * menu grid layout) reflect the change on their very next draw call. */
    fox_theme_set(index == 1);
}

#define BATTERY_VIEW_COUNT 6
static const char* const battery_view_text[BATTERY_VIEW_COUNT] =
    {"Bar", "%", "Inv. %", "Retro 3", "Retro 5", "Bar %"};
static const uint32_t battery_view_value[BATTERY_VIEW_COUNT] = {
    DISPLAY_BATTERY_BAR,
    DISPLAY_BATTERY_PERCENT,
    DISPLAY_BATTERY_INVERTED_PERCENT,
    DISPLAY_BATTERY_RETRO_3,
    DISPLAY_BATTERY_RETRO_5,
    DISPLAY_BATTERY_BAR_PERCENT};

static void desktop_settings_scene_start_var_list_enter_callback(void* context, uint32_t index) {
    DesktopSettingsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void desktop_settings_scene_start_battery_view_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, battery_view_text[index]);
    app->settings.displayBatteryPercentage = index;
    /* Save and push to desktop handled at app exit by desktop_settings_app(),
     * same as display_clock and all other settings — making it instant. */
}

static void desktop_settings_scene_start_clock_enable_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, clock_enable_text[index]);
    app->settings.display_clock = index;
}

void desktop_settings_scene_start_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    VariableItemList* list = app->variable_item_list;
    VariableItem* item;
    uint8_t value_index;

    variable_item_list_add(list, "Security & Privacy", 0, NULL, NULL);
    variable_item_list_add(list, "Custom Wallpaper", 0, NULL, NULL);

    item = variable_item_list_add(
        list, "Menu Style", MENU_STYLE_COUNT,
        desktop_settings_scene_start_menu_style_changed, app);
    {
        uint8_t ms_idx = fox_theme_is_active() ? 1u : 0u;
        app->settings.menu_theme = ms_idx;
        variable_item_set_current_value_index(item, ms_idx);
        variable_item_set_current_value_text(item, menu_style_text[ms_idx]);
    }

    variable_item_list_add(list, "Change Flipper Name", 0, NULL, app);

    item = variable_item_list_add(
        list, "Battery View", BATTERY_VIEW_COUNT,
        desktop_settings_scene_start_battery_view_changed, app);
    value_index = value_index_uint32(
        app->settings.displayBatteryPercentage, battery_view_value, BATTERY_VIEW_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, battery_view_text[value_index]);

    item = variable_item_list_add(
        list, "Show Clock", CLOCK_ENABLE_COUNT,
        desktop_settings_scene_start_clock_enable_changed, app);
    value_index =
        value_index_uint32(app->settings.display_clock, clock_enable_value, CLOCK_ENABLE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, clock_enable_text[value_index]);

    variable_item_list_add(list, "Favourite - Left Short",  0, NULL, NULL);
    variable_item_list_add(list, "Favourite - Left Long",   0, NULL, NULL);
    variable_item_list_add(list, "Favourite - Right Short", 0, NULL, NULL);
    variable_item_list_add(list, "Favourite - Right Long",  0, NULL, NULL);
    variable_item_list_add(list, "Favourite - Ok Long",     0, NULL, NULL);

    variable_item_list_set_enter_callback(
        list, desktop_settings_scene_start_var_list_enter_callback, app);

    /* Restore the item that was selected before navigating into a sub-scene.
     * On a fresh app launch the scene_state is 0 so the list starts at the top.
     * On return from a sub-scene it holds the index of the item that was pressed. */
    uint32_t saved_pos = scene_manager_get_scene_state(
        app->scene_manager, DesktopSettingsAppSceneStart);
    variable_item_list_set_selected_item(list, (uint8_t)saved_pos);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_start_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        /* The custom event value IS the VariableItemList item index.
         * Save it now so on_enter can restore the scroll position when the
         * user returns from a sub-scene (Back key from Favourite editor etc.).
         * On a fresh app launch the state is 0 so the list starts at the top. */
        scene_manager_set_scene_state(
            app->scene_manager, DesktopSettingsAppSceneStart, event.event);

        switch(event.event) {
        case DesktopSettingsPinSetup:
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppScenePinMenu);
            break;
        case DesktopSettingsMenuStyle:
            break;
        case DesktopSettingsWallpaper:
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneWallpaperSetup);
            break;
        case DesktopSettingsChangeName:
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneChangeName);
            break;
        case DesktopSettingsFavoriteLeftShort:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneFavorite,
                SCENE_STATE_SET_FAVORITE_APP | FavoriteAppLeftShort);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneFavorite);
            break;
        case DesktopSettingsFavoriteLeftLong:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneFavorite,
                SCENE_STATE_SET_FAVORITE_APP | FavoriteAppLeftLong);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneFavorite);
            break;
        case DesktopSettingsFavoriteRightShort:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneFavorite,
                SCENE_STATE_SET_FAVORITE_APP | FavoriteAppRightShort);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneFavorite);
            break;
        case DesktopSettingsFavoriteRightLong:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneFavorite,
                SCENE_STATE_SET_FAVORITE_APP | FavoriteAppRightLong);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneFavorite);
            break;
        case DesktopSettingsFavoriteOkLong:
            scene_manager_set_scene_state(
                app->scene_manager, DesktopSettingsAppSceneFavorite,
                SCENE_STATE_SET_FAVORITE_APP | FavoriteAppOkLong);
            scene_manager_next_scene(app->scene_manager, DesktopSettingsAppSceneFavorite);
            break;
        default:
            break;
        }
        consumed = true;
    }
    return consumed;
}

void desktop_settings_scene_start_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    variable_item_list_reset(app->variable_item_list);
    desktop_settings_save(&app->settings);
    fox_theme_set(app->settings.menu_theme == 1);
}
