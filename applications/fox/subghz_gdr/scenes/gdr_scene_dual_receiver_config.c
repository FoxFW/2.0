// scenes/gdr_scene_dual_receiver_config.c
#include "../gdr_app_i.h"

#ifdef ENABLE_DUAL_RX_SCENE

#define TAG "GDRSceneDualCfg"

typedef enum {
    GDRDualConfigExtFrequency,
    GDRDualConfigExtPreset,
    GDRDualConfigIntFrequency,
    GDRDualConfigIntPreset,
    GDRDualConfigSwapRoles,
} GDRDualConfigItem;

static void gdr_dual_config_set_preset_item(
    GDRApp* app,
    VariableItem* item,
    uint8_t index) {
    variable_item_set_current_value_index(item, index);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, index));
}

static uint8_t gdr_dual_config_freq_index(GDRApp* app, uint32_t frequency) {
    uint8_t count = (uint8_t)subghz_setting_get_frequency_count(app->setting);
    for(uint8_t i = 0; i < count; i++) {
        if(subghz_setting_get_frequency(app->setting, i) == frequency) {
            return i;
        }
    }
    return subghz_setting_get_frequency_default_index(app->setting);
}

static void gdr_dual_config_set_freq_text(VariableItem* item, uint32_t frequency) {
    char text_buf[10] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        (unsigned long)(frequency / 1000000UL),
        (unsigned long)((frequency % 1000000UL) / 10000UL));
    variable_item_set_current_value_text(item, text_buf);
}

static void gdr_dual_config_set_freq_a(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->dual_freq_a = subghz_setting_get_frequency(app->setting, index);
    gdr_dual_config_set_freq_text(item, app->dual_freq_a);
}

static void gdr_dual_config_set_freq_b(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->dual_freq_b = subghz_setting_get_frequency(app->setting, index);
    gdr_dual_config_set_freq_text(item, app->dual_freq_b);
}

static void gdr_dual_config_set_preset_a(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->dual_preset_a = index;
    gdr_dual_config_set_preset_item(app, item, index);
}

static void gdr_dual_config_set_preset_b(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->dual_preset_b = index;
    gdr_dual_config_set_preset_item(app, item, index);
}

static void gdr_dual_config_enter_callback(void* context, uint32_t index) {
    GDRApp* app = context;
    if(index != GDRDualConfigSwapRoles) {
        return;
    }

    uint8_t preset_a = app->dual_preset_a;
    app->dual_preset_a = app->dual_preset_b;
    app->dual_preset_b = preset_a;
    gdr_dual_config_set_preset_item(
        app,
        variable_item_list_get(app->variable_item_list, GDRDualConfigExtPreset),
        app->dual_preset_a);
    gdr_dual_config_set_preset_item(
        app,
        variable_item_list_get(app->variable_item_list, GDRDualConfigIntPreset),
        app->dual_preset_b);
    notification_message(app->notifications, &sequence_success);
}

void gdr_scene_dual_receiver_config_on_enter(void* context) {
    furi_check(context);
    GDRApp* app = context;
    VariableItem* item;
    uint8_t value_index;

    if(!gdr_ensure_variable_item_list(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    uint8_t freq_count = (uint8_t)subghz_setting_get_frequency_count(app->setting);
    uint8_t preset_count = (uint8_t)subghz_setting_get_preset_count(app->setting);

    // Chain A (external)
    item = variable_item_list_add(
        app->variable_item_list, "Ext Freq:", freq_count, gdr_dual_config_set_freq_a, app);
    value_index = gdr_dual_config_freq_index(app, app->dual_freq_a);
    variable_item_set_current_value_index(item, value_index);
    gdr_dual_config_set_freq_text(
        item, subghz_setting_get_frequency(app->setting, value_index));

    item = variable_item_list_add(
        app->variable_item_list,
        "Ext Preset:",
        preset_count,
        gdr_dual_config_set_preset_a,
        app);
    variable_item_set_current_value_index(item, app->dual_preset_a);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, app->dual_preset_a));

    // Chain B (internal)
    item = variable_item_list_add(
        app->variable_item_list, "Int Freq:", freq_count, gdr_dual_config_set_freq_b, app);
    value_index = gdr_dual_config_freq_index(app, app->dual_freq_b);
    variable_item_set_current_value_index(item, value_index);
    gdr_dual_config_set_freq_text(
        item, subghz_setting_get_frequency(app->setting, value_index));

    item = variable_item_list_add(
        app->variable_item_list,
        "Int Preset:",
        preset_count,
        gdr_dual_config_set_preset_b,
        app);
    variable_item_set_current_value_index(item, app->dual_preset_b);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, app->dual_preset_b));

    item = variable_item_list_add(
        app->variable_item_list, "Swap roles", 1, NULL, NULL);
    variable_item_set_current_value_text(item, "Press OK");
    variable_item_list_set_enter_callback(
        app->variable_item_list, gdr_dual_config_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewVariableItemList);
}

bool gdr_scene_dual_receiver_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gdr_scene_dual_receiver_config_on_exit(void* context) {
    furi_check(context);
    GDRApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}

#endif // ENABLE_DUAL_RX_SCENE
