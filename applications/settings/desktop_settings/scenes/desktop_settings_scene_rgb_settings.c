#include <furi.h>
#include <lib/toolbox/value_index.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

// Mirrors the RGB Mod Settings sub-screen in notification_settings_app.c
// (Settings > LCD and Notifications), just surfaced directly from Fox
// Settings for discoverability - reuses the same rgb_backlight_* functions
// and NotificationSettings.rgb storage, so both entry points stay in sync.

#define RGB_BACKLIGHT_INSTALLED_COUNT 2
static const char* const rgb_backlight_installed_text[RGB_BACKLIGHT_INSTALLED_COUNT] = {
    "OFF",
    "ON",
};
static const bool rgb_backlight_installed_value[RGB_BACKLIGHT_INSTALLED_COUNT] = {false, true};

// White Backlight ON keeps the stock white LED running alongside the RGB
// LEDs (original FoxFW behavior); OFF drives RGB only, same as Momentum.
#define RGB_BACKLIGHT_WHITE_MODE_COUNT 2
static const char* const rgb_backlight_white_mode_text[RGB_BACKLIGHT_WHITE_MODE_COUNT] = {
    "OFF",
    "ON",
};
static const bool rgb_backlight_white_mode_value[RGB_BACKLIGHT_WHITE_MODE_COUNT] = {false, true};

#define RGB_BACKLIGHT_RAINBOW_MODE_COUNT 3
static const char* const rgb_backlight_rainbow_mode_text[RGB_BACKLIGHT_RAINBOW_MODE_COUNT] = {
    "OFF",
    "Rainbow",
    "Wave",
};
static const uint32_t rgb_backlight_rainbow_mode_value[RGB_BACKLIGHT_RAINBOW_MODE_COUNT] = {
    0,
    1,
    2};

#define RGB_BACKLIGHT_RAINBOW_SPEED_COUNT 10
static const char* const rgb_backlight_rainbow_speed_text[RGB_BACKLIGHT_RAINBOW_SPEED_COUNT] = {
    "0.1s",
    "0.2s",
    "0.3s",
    "0.4s",
    "0.5s",
    "0.6s",
    "0.7",
    "0.8",
    "0.9",
    "1s",
};
static const uint32_t rgb_backlight_rainbow_speed_value[RGB_BACKLIGHT_RAINBOW_SPEED_COUNT] = {
    100,
    200,
    300,
    400,
    500,
    600,
    700,
    800,
    900,
    1000,
};

#define RGB_BACKLIGHT_RAINBOW_STEP_COUNT 3
static const char* const rgb_backlight_rainbow_step_text[RGB_BACKLIGHT_RAINBOW_STEP_COUNT] = {
    "1",
    "2",
    "3",
};
static const uint32_t rgb_backlight_rainbow_step_value[RGB_BACKLIGHT_RAINBOW_STEP_COUNT] = {
    1,
    2,
    3,
};

#define RGB_BACKLIGHT_RAINBOW_WIDE_COUNT 3
static const char* const rgb_backlight_rainbow_wide_text[RGB_BACKLIGHT_RAINBOW_WIDE_COUNT] = {
    "1",
    "2",
    "3",
};
static const uint32_t rgb_backlight_rainbow_wide_value[RGB_BACKLIGHT_RAINBOW_WIDE_COUNT] = {
    30,
    40,
    50,
};

static void desktop_settings_scene_rgb_installed_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_installed_text[index]);
    app->notification->settings.rgb.rgb_backlight_installed = rgb_backlight_installed_value[index];
    set_rgb_backlight_installed_variable(rgb_backlight_installed_value[index]);

    if(index == 0) {
        rgb_backlight_set_led_static_color(2, 0);
        rgb_backlight_set_led_static_color(1, 0);
        rgb_backlight_set_led_static_color(0, 0);
        SK6805_update();
        rainbow_timer_stop(app->notification);
    } else {
        if(app->notification->settings.rgb.rainbow_mode > 0) {
            rainbow_timer_starter(app->notification);
        } else {
            rgb_backlight_set_led_static_color(2, app->notification->settings.rgb.led_2_color_index);
            rgb_backlight_set_led_static_color(1, app->notification->settings.rgb.led_1_color_index);
            rgb_backlight_set_led_static_color(0, app->notification->settings.rgb.led_0_color_index);
            rgb_backlight_update(
                app->notification->settings.display_brightness * app->notification->current_night_shift);
        }
    }

    for(int i = 1; i < 10; i++) {
        VariableItem* t_item = variable_item_list_get(app->variable_item_list, i);
        variable_item_set_locked(t_item, index == 0, "RGB\nOFF!");
    }
    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_white_mode_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_white_mode_text[index]);
    app->notification->settings.rgb.white_backlight_mode = rgb_backlight_white_mode_value[index];
    set_rgb_backlight_white_mode_variable(rgb_backlight_white_mode_value[index]);

    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_led_2_color_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_get_color_text(index));
    app->notification->settings.rgb.led_2_color_index = index;

    if(!furi_timer_is_running(app->notification->rainbow_timer)) {
        rgb_backlight_set_led_static_color(2, index);
        rgb_backlight_update(
            app->notification->settings.display_brightness * app->notification->current_night_shift);
    }
    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_led_1_color_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_get_color_text(index));
    app->notification->settings.rgb.led_1_color_index = index;

    if(!furi_timer_is_running(app->notification->rainbow_timer)) {
        rgb_backlight_set_led_static_color(1, index);
        rgb_backlight_update(
            app->notification->settings.display_brightness * app->notification->current_night_shift);
    }
    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_led_0_color_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_get_color_text(index));
    app->notification->settings.rgb.led_0_color_index = index;

    if(!furi_timer_is_running(app->notification->rainbow_timer)) {
        rgb_backlight_set_led_static_color(0, index);
        rgb_backlight_update(
            app->notification->settings.display_brightness * app->notification->current_night_shift);
    }
    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_rainbow_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_mode_text[index]);
    app->notification->settings.rgb.rainbow_mode = rgb_backlight_rainbow_mode_value[index];

    if(index == 0) {
        rgb_backlight_set_led_static_color(2, app->notification->settings.rgb.led_2_color_index);
        rgb_backlight_set_led_static_color(1, app->notification->settings.rgb.led_1_color_index);
        rgb_backlight_set_led_static_color(0, app->notification->settings.rgb.led_0_color_index);
        rgb_backlight_update(
            app->notification->settings.display_brightness * app->notification->current_night_shift);
        rainbow_timer_stop(app->notification);
    } else {
        rainbow_timer_starter(app->notification);
    }
    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_rainbow_speed_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_speed_text[index]);
    app->notification->settings.rgb.rainbow_speed_ms = rgb_backlight_rainbow_speed_value[index];

    rainbow_timer_starter(app->notification);
    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_rainbow_step_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_step_text[index]);
    app->notification->settings.rgb.rainbow_step = rgb_backlight_rainbow_step_value[index];

    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_rainbow_saturation_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    // saturation must be 1..255, so we do (0..254)+1
    uint8_t index = variable_item_get_current_value_index(item) + 1;
    char valtext[4] = {};
    snprintf(valtext, sizeof(valtext), "%d", index);
    variable_item_set_current_value_text(item, valtext);
    app->notification->settings.rgb.rainbow_saturation = index;

    notification_message_save_settings(app->notification);
}

static void desktop_settings_scene_rgb_rainbow_wide_changed(VariableItem* item) {
    DesktopSettingsApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_wide_text[index]);
    app->notification->settings.rgb.rainbow_wide = rgb_backlight_rainbow_wide_value[index];

    notification_message_save_settings(app->notification);
}

void desktop_settings_scene_rgb_settings_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    VariableItemList* list = app->variable_item_list;
    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        list,
        "RGB backlight installed",
        RGB_BACKLIGHT_INSTALLED_COUNT,
        desktop_settings_scene_rgb_installed_changed,
        app);
    value_index = value_index_bool(
        app->notification->settings.rgb.rgb_backlight_installed,
        rgb_backlight_installed_value,
        RGB_BACKLIGHT_INSTALLED_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_installed_text[value_index]);

    item = variable_item_list_add(
        list,
        "White Backlight",
        RGB_BACKLIGHT_WHITE_MODE_COUNT,
        desktop_settings_scene_rgb_white_mode_changed,
        app);
    value_index = value_index_bool(
        app->notification->settings.rgb.white_backlight_mode,
        rgb_backlight_white_mode_value,
        RGB_BACKLIGHT_WHITE_MODE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_white_mode_text[value_index]);
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    // We (humans) number LEDs left to right as 1..3, hardware order is 2..0
    item = variable_item_list_add(
        list,
        "LED 1 Color",
        rgb_backlight_get_color_count(),
        desktop_settings_scene_rgb_led_2_color_changed,
        app);
    value_index = app->notification->settings.rgb.led_2_color_index;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_get_color_text(value_index));
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        "LED 2 Color",
        rgb_backlight_get_color_count(),
        desktop_settings_scene_rgb_led_1_color_changed,
        app);
    value_index = app->notification->settings.rgb.led_1_color_index;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_get_color_text(value_index));
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        "LED 3 Color",
        rgb_backlight_get_color_count(),
        desktop_settings_scene_rgb_led_0_color_changed,
        app);
    value_index = app->notification->settings.rgb.led_0_color_index;
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_get_color_text(value_index));
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        "Effects",
        RGB_BACKLIGHT_RAINBOW_MODE_COUNT,
        desktop_settings_scene_rgb_rainbow_changed,
        app);
    value_index = value_index_uint32(
        app->notification->settings.rgb.rainbow_mode,
        rgb_backlight_rainbow_mode_value,
        RGB_BACKLIGHT_RAINBOW_MODE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_mode_text[value_index]);
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        " . Speed",
        RGB_BACKLIGHT_RAINBOW_SPEED_COUNT,
        desktop_settings_scene_rgb_rainbow_speed_changed,
        app);
    value_index = value_index_uint32(
        app->notification->settings.rgb.rainbow_speed_ms,
        rgb_backlight_rainbow_speed_value,
        RGB_BACKLIGHT_RAINBOW_SPEED_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_speed_text[value_index]);
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        " . Color step",
        RGB_BACKLIGHT_RAINBOW_STEP_COUNT,
        desktop_settings_scene_rgb_rainbow_step_changed,
        app);
    value_index = value_index_uint32(
        app->notification->settings.rgb.rainbow_step,
        rgb_backlight_rainbow_step_value,
        RGB_BACKLIGHT_RAINBOW_STEP_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_step_text[value_index]);
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        " . Saturation",
        255,
        desktop_settings_scene_rgb_rainbow_saturation_changed,
        app);
    value_index = app->notification->settings.rgb.rainbow_saturation;
    variable_item_set_current_value_index(item, value_index);
    char valtext[4] = {};
    snprintf(valtext, sizeof(valtext), "%d", value_index);
    variable_item_set_current_value_text(item, valtext);
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    item = variable_item_list_add(
        list,
        " . Wave wide",
        RGB_BACKLIGHT_RAINBOW_WIDE_COUNT,
        desktop_settings_scene_rgb_rainbow_wide_changed,
        app);
    value_index = value_index_uint32(
        app->notification->settings.rgb.rainbow_wide,
        rgb_backlight_rainbow_wide_value,
        RGB_BACKLIGHT_RAINBOW_WIDE_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, rgb_backlight_rainbow_wide_text[value_index]);
    variable_item_set_locked(
        item, app->notification->settings.rgb.rgb_backlight_installed == 0, "RGB MOD \nOFF!");

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_rgb_settings_on_event(void* context, SceneManagerEvent event) {
    DesktopSettingsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        notification_message_save_settings(app->notification);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void desktop_settings_scene_rgb_settings_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    variable_item_list_reset(app->variable_item_list);
    furi_record_close(RECORD_NOTIFICATION);
}
