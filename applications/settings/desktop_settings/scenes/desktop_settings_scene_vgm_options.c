#include <furi.h>
#include <screen_color/screen_color_settings.h>

#include "../desktop_settings_app.h"
#include "desktop_settings_scene.h"

/* VGM Options (ported from Momentum Firmware): sets the foreground/
 * background color used when this device's screen is streamed over RPC
 * (qFlipper / companion screen-mirroring tools). Purely cosmetic to the
 * receiving client - doesn't affect the device's own display. */

enum VarItemListIndex {
    VarItemListIndexForeground,
    VarItemListIndexBackground,
};

typedef struct {
    const char* name;
    uint8_t r, g, b;
} VgmColor;

/* Index 0-2 are modes (no fixed RGB); index >= 3 are fixed custom colors. */
static const VgmColor vgm_colors[] = {
    {"Default", 0, 0, 0},
    {"Rainbow", 0, 0, 0},
    {"RgbMod", 0, 0, 0},
    {"Black", 0, 0, 0},
    {"Orange", 255, 130, 0},
    {"Red", 255, 0, 0},
    {"Maroon", 128, 0, 0},
    {"Yellow", 255, 255, 0},
    {"Olive", 128, 128, 0},
    {"Lime", 0, 255, 0},
    {"Green", 0, 128, 0},
    {"Aqua", 0, 255, 127},
    {"Cyan", 0, 210, 210},
    {"Azure", 0, 127, 255},
    {"Teal", 0, 128, 128},
    {"Blue", 0, 0, 255},
    {"Navy", 0, 0, 128},
    {"Purple", 128, 0, 128},
    {"Fuchsia", 255, 0, 255},
    {"Pink", 255, 105, 180},
    {"Brown", 165, 42, 42},
    {"White", 255, 255, 255},
};
#define VGM_COLORS_COUNT (sizeof(vgm_colors) / sizeof(VgmColor))

static ScreenColorSettings s_screen_color;

static uint8_t desktop_settings_scene_vgm_options_index_for(uint8_t mode, uint8_t r, uint8_t g, uint8_t b) {
    if(mode == ScreenColorModeDefault) return 0;
    if(mode == ScreenColorModeRainbow) return 1;
    if(mode == ScreenColorModeRgbBacklight) return 2;
    for(size_t i = 3; i < VGM_COLORS_COUNT; i++) {
        if(vgm_colors[i].r == r && vgm_colors[i].g == g && vgm_colors[i].b == b) return i;
    }
    return 0;
}

static void desktop_settings_scene_vgm_options_apply(uint8_t index, uint8_t* mode, uint8_t* r, uint8_t* g, uint8_t* b) {
    if(index == 0) {
        *mode = ScreenColorModeDefault;
    } else if(index == 1) {
        *mode = ScreenColorModeRainbow;
    } else if(index == 2) {
        *mode = ScreenColorModeRgbBacklight;
    } else {
        *mode = ScreenColorModeCustom;
    }
    *r = vgm_colors[index].r;
    *g = vgm_colors[index].g;
    *b = vgm_colors[index].b;
}

static void desktop_settings_scene_vgm_options_foreground_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, vgm_colors[index].name);
    desktop_settings_scene_vgm_options_apply(
        index, &s_screen_color.fg_mode, &s_screen_color.fg_r, &s_screen_color.fg_g, &s_screen_color.fg_b);
    screen_color_settings_save(&s_screen_color);
}

static void desktop_settings_scene_vgm_options_background_changed(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, vgm_colors[index].name);
    desktop_settings_scene_vgm_options_apply(
        index, &s_screen_color.bg_mode, &s_screen_color.bg_r, &s_screen_color.bg_g, &s_screen_color.bg_b);
    screen_color_settings_save(&s_screen_color);
}

void desktop_settings_scene_vgm_options_on_enter(void* context) {
    DesktopSettingsApp* app = context;
    VariableItemList* list = app->variable_item_list;
    VariableItem* item;
    uint8_t value_index;

    screen_color_settings_load(&s_screen_color);

    item = variable_item_list_add(
        list,
        "Foreground",
        VGM_COLORS_COUNT,
        desktop_settings_scene_vgm_options_foreground_changed,
        app);
    value_index = desktop_settings_scene_vgm_options_index_for(
        s_screen_color.fg_mode, s_screen_color.fg_r, s_screen_color.fg_g, s_screen_color.fg_b);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, vgm_colors[value_index].name);

    item = variable_item_list_add(
        list,
        "Background",
        VGM_COLORS_COUNT,
        desktop_settings_scene_vgm_options_background_changed,
        app);
    value_index = desktop_settings_scene_vgm_options_index_for(
        s_screen_color.bg_mode, s_screen_color.bg_r, s_screen_color.bg_g, s_screen_color.bg_b);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, vgm_colors[value_index].name);

    view_dispatcher_switch_to_view(app->view_dispatcher, DesktopSettingsAppViewVarItemList);
}

bool desktop_settings_scene_vgm_options_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void desktop_settings_scene_vgm_options_on_exit(void* context) {
    DesktopSettingsApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
