// scenes/gdr_scene_shield_receiver_config.c
#include "../gdr_app_i.h"

#ifdef ENABLE_SHIELD_RX_SCENE

#define TAG "GDRSceneShieldCfg"

#define SHIELD_TX_OFFSET_COUNT 12U
static const char* const shield_tx_offset_text[SHIELD_TX_OFFSET_COUNT] = {
    "+75 kHz",
    "+100 kHz",
    "+150 kHz",
    "+200 kHz",
    "+250 kHz",
    "+300 kHz",
    "-75 kHz",
    "-100 kHz",
    "-150 kHz",
    "-200 kHz",
    "-250 kHz",
    "-300 kHz",
};

typedef enum {
    GDRShieldConfigFrequency,
    GDRShieldConfigPreset,
    GDRShieldConfigTxOffset,
    GDRShieldConfigTxPower,
} GDRShieldConfigItem;

static uint8_t gdr_shield_config_freq_index(GDRApp* app, uint32_t frequency) {
    uint8_t count = (uint8_t)subghz_setting_get_frequency_count(app->setting);
    for(uint8_t i = 0; i < count; i++) {
        if(subghz_setting_get_frequency(app->setting, i) == frequency) {
            return i;
        }
    }
    return subghz_setting_get_frequency_default_index(app->setting);
}

static void gdr_shield_config_set_freq_text(VariableItem* item, uint32_t frequency) {
    char text_buf[10] = {0};
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        (unsigned long)(frequency / 1000000UL),
        (unsigned long)((frequency % 1000000UL) / 10000UL));
    variable_item_set_current_value_text(item, text_buf);
}

static void gdr_shield_config_set_frequency(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->shield_freq = subghz_setting_get_frequency(app->setting, index);
    gdr_shield_config_set_freq_text(item, app->shield_freq);
}

static void gdr_shield_config_set_preset(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->shield_preset_index = index;
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, index));
}

static void gdr_shield_config_set_tx_offset(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= SHIELD_TX_OFFSET_COUNT) {
        index = 3;
    }
    app->shield_tx_offset_index = index;
    variable_item_set_current_value_text(item, shield_tx_offset_text[index]);
}

#define SHIELD_TX_POWER_COUNT 9
static const char* const shield_tx_power_text[SHIELD_TX_POWER_COUNT] = {
    "Preset",
    "10dBm +",
    "7dBm",
    "5dBm",
    "0dBm",
    "-10dBm",
    "-15dBm",
    "-20dBm",
    "-30dBm",
};

static void gdr_shield_config_set_tx_power(VariableItem* item) {
    GDRApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->shield_tx_power = index;
    variable_item_set_current_value_text(item, shield_tx_power_text[index]);
}

void gdr_scene_shield_receiver_config_on_enter(void* context) {
    furi_check(context);
    GDRApp* app = context;
    VariableItem* item;

    if(!gdr_ensure_variable_item_list(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    variable_item_list_reset(app->variable_item_list);

    item = variable_item_list_add(
        app->variable_item_list,
        "RX Frequency",
        subghz_setting_get_frequency_count(app->setting),
        gdr_shield_config_set_frequency,
        app);
    variable_item_set_current_value_index(
        item, gdr_shield_config_freq_index(app, app->shield_freq));
    gdr_shield_config_set_freq_text(item, app->shield_freq);

    item = variable_item_list_add(
        app->variable_item_list,
        "RX Modulation",
        subghz_setting_get_preset_count(app->setting),
        gdr_shield_config_set_preset,
        app);
    if(app->shield_preset_index >= subghz_setting_get_preset_count(app->setting)) {
        app->shield_preset_index = 0;
    }
    variable_item_set_current_value_index(item, app->shield_preset_index);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(app->setting, app->shield_preset_index));

    item = variable_item_list_add(
        app->variable_item_list,
        "Jam TX Offset",
        SHIELD_TX_OFFSET_COUNT,
        gdr_shield_config_set_tx_offset,
        app);
    if(app->shield_tx_offset_index >= SHIELD_TX_OFFSET_COUNT) {
        app->shield_tx_offset_index = 3;
    }
    variable_item_set_current_value_index(item, app->shield_tx_offset_index);
    variable_item_set_current_value_text(item, shield_tx_offset_text[app->shield_tx_offset_index]);

    item = variable_item_list_add(
        app->variable_item_list,
        "Shield TX Power",
        SHIELD_TX_POWER_COUNT,
        gdr_shield_config_set_tx_power,
        app);
    if(app->shield_tx_power >= SHIELD_TX_POWER_COUNT) {
        app->shield_tx_power = 0U;
    }
    variable_item_set_current_value_index(item, app->shield_tx_power);
    variable_item_set_current_value_text(item, shield_tx_power_text[app->shield_tx_power]);

    view_dispatcher_switch_to_view(app->view_dispatcher, GDRViewVariableItemList);
}

bool gdr_scene_shield_receiver_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gdr_scene_shield_receiver_config_on_exit(void* context) {
    furi_check(context);
    GDRApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}

#endif // ENABLE_SHIELD_RX_SCENE
