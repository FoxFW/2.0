#include "../subghz_i.h"
#include "../subghz_modulation_filter.h"
#include "../helpers/subghz_lib_ext_compat.h"

/* Path and key mirrors the FA FAP so modulation changes here propagate to FA */
#define LAST_SETTINGS_FILE EXT_PATH("subghz/assets/last_subghz.settings")
#define FA_PRESET_IDX_KEY  "FAPresetIndex"

static void subghz_config_sync_preset_to_fa(int preset_index) {
    if(preset_index < 0) return;
    Storage* s = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(s);
    if(flipper_format_file_open_existing(ff, LAST_SETTINGS_FILE)) {
        uint32_t pi = (uint32_t)preset_index;
        if(!flipper_format_update_uint32(ff, FA_PRESET_IDX_KEY, &pi, 1)) {
            flipper_format_rewind(ff);
            flipper_format_write_uint32(ff, FA_PRESET_IDX_KEY, &pi, 1);
        }
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}
#include "../subghz_modulation_filter.h"
#include <lib/toolbox/value_index.h>
#include <furi/core/memmgr.h>
#include <math.h>

#define TAG "SubGhzSceneReceiverConfig"

enum SubGhzSettingIndex {
    SubGhzSettingIndexFrequency,
    SubGhzSettingIndexModulation,
    SubGhzSettingIndexHopping,
    SubGhzSettingIndexBinRAW,
    SubGhzSettingIndexSound,
    SubGhzSettingIndexRAWThresholdRSSI,
};

#define RAW_THRESHOLD_RSSI_COUNT 11
const char* const raw_threshold_rssi_text[RAW_THRESHOLD_RSSI_COUNT] = {
    "-----",
    "-85.0",
    "-80.0",
    "-75.0",
    "-70.0",
    "-65.0",
    "-60.0",
    "-55.0",
    "-50.0",
    "-45.0",
    "-40.0",

};
const float raw_threshold_rssi_value[RAW_THRESHOLD_RSSI_COUNT] = {
    -90.0f,
    -85.0f,
    -80.0f,
    -75.0f,
    -70.0f,
    -65.0f,
    -60.0f,
    -55.0f,
    -50.0f,
    -45.0f,
    -40.0f,
};

/* Trimmed-down Hopping: a plain On/Off toggle (COMBO_BOX_COUNT/combobox_text,
 * defined below) rather than the full 12-level RSSI-threshold picker stock
 * exposes. We still drive the exact same stock hopper engine (helpers/
 * subghz_txrx.c: subghz_txrx_hopper_update() et al - untouched, already
 * fully implemented and already linked in regardless of this toggle, so
 * enabling it costs no extra flash), just with a single fixed "stay"
 * threshold instead of a user-configurable one. -90dBm matches this app's
 * pre-existing default (see hopping_threshold's default in subghz_last_
 * settings.c) - close enough to the CC1101's noise floor to treat almost
 * any real transmission as "stay", without configurable knobs to expose. */
#define SUBGHZ_GARAGE_HOPPING_RSSI_THRESHOLD (-90.0f)

#define COMBO_BOX_COUNT 2

const uint32_t speaker_value[COMBO_BOX_COUNT] = {
    SubGhzSpeakerStateShutdown,
    SubGhzSpeakerStateEnable,
};

const uint32_t bin_raw_value[COMBO_BOX_COUNT] = {
    SubGhzProtocolFlag_Decodable,
    SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_BinRAW,
};

const char* const combobox_text[COMBO_BOX_COUNT] = {
    "OFF",
    "ON",
};

uint8_t subghz_scene_receiver_config_next_frequency(const uint32_t value, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);

    uint8_t index = 0;
    for(size_t i = 0; i < subghz_setting_get_frequency_count(setting); i++) {
        if(value == subghz_setting_get_frequency(setting, i)) {
            index = i;
            break;
        } else {
            index = subghz_setting_get_frequency_default_index(setting);
        }
    }
    return index;
}


/* How many presets are currently enabled (minimum 1 to avoid 0-item list) */
static size_t mod_filter_enabled_count(SubGhz* subghz) {
    if(!subghz->modulation_filter) return subghz_setting_get_preset_count(subghz_txrx_get_setting(subghz->txrx));
    SubGhzSetting* s = subghz_txrx_get_setting(subghz->txrx);
    size_t total = subghz_setting_get_preset_count(s);
    size_t n = 0;
    for(size_t i = 0; i < total; i++)
        if(subghz_modulation_filter_is_enabled(subghz->modulation_filter, i)) n++;
    return n > 0 ? n : 1;
}

/* Actual preset index for the nth ENABLED preset */
static uint8_t mod_filter_nth_actual(SubGhz* subghz, uint8_t nth) {
    if(!subghz->modulation_filter) return nth;
    SubGhzSetting* s = subghz_txrx_get_setting(subghz->txrx);
    size_t total = subghz_setting_get_preset_count(s);
    uint8_t found = 0;
    for(size_t i = 0; i < total; i++) {
        if(subghz_modulation_filter_is_enabled(subghz->modulation_filter, i)) {
            if(found == nth) return (uint8_t)i;
            found++;
        }
    }
    return 0;
}

/* Display index (in the visible list) for a given actual preset index.
 * Returns 0 if the preset is disabled (auto-selects first enabled). */
static uint8_t mod_filter_display_idx(SubGhz* subghz, uint8_t actual_idx) {
    if(!subghz->modulation_filter) return actual_idx;
    SubGhzSetting* s = subghz_txrx_get_setting(subghz->txrx);
    size_t total = subghz_setting_get_preset_count(s);
    uint8_t display = 0;
    for(size_t i = 0; i < total; i++) {
        if(!subghz_modulation_filter_is_enabled(subghz->modulation_filter, i)) continue;
        if(i == actual_idx) return display;
        display++;
    }
    return 0; /* disabled or not found — default to first enabled */
}

uint8_t subghz_scene_receiver_config_next_preset(const char* preset_name, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    uint8_t index = 0;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);

    for(size_t i = 0; i < subghz_setting_get_preset_count(setting); i++) {
        if(!strcmp(subghz_setting_get_preset_name(setting, i), preset_name)) {
            index = i;
            break;
        }
    }
    return index;
}

uint8_t subghz_scene_receiver_config_hopper_value_index(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;

    if(subghz_txrx_hopper_get_state(subghz->txrx) == SubGhzHopperStateOFF) {
        return 0;
    } else {
        variable_item_set_current_value_text(
            (VariableItem*)scene_manager_get_scene_state(
                subghz->scene_manager, SubGhzSceneReceiverConfig),
            " -----");
        return 1;
    }
}

static void subghz_scene_receiver_config_set_frequency(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    FURI_LOG_I(TAG, "set_frequency: enter, index=%d", (int)index);

    if(subghz_txrx_hopper_get_state(subghz->txrx) == SubGhzHopperStateOFF) {
        char text_buf[10] = {0};
        uint32_t frequency = subghz_setting_get_frequency(setting, index);
        SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            frequency / 1000000,
            (frequency % 1000000) / 10000);
        variable_item_set_current_value_text(item, text_buf);

        //Set TX Power
        subghz_txrx_set_tx_power(preset.data, preset.data_size, subghz->tx_power);

        //Set the preset now.
        subghz_txrx_set_preset(
            subghz->txrx,
            furi_string_get_cstr(preset.name),
            frequency,
            preset.data,
            preset.data_size);



        subghz->last_settings->frequency = frequency;
        subghz_garage_setting_mark_default_frequency(setting, frequency);
    } else {
        variable_item_set_current_value_index(
            item, subghz_setting_get_frequency_default_index(setting));
    }
    FURI_LOG_I(TAG, "set_frequency: done");
}

static void subghz_scene_receiver_config_set_preset(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    /* display_idx is the position in the ENABLED-ONLY list (0..enabled_count-1).
     * Convert to the actual preset index in SubGhzSetting. */
    uint8_t display_idx = variable_item_get_current_value_index(item);
    uint8_t index       = mod_filter_nth_actual(subghz, display_idx);
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    FURI_LOG_I(TAG, "set_preset: enter, display_idx=%d actual_idx=%d", (int)display_idx, (int)index);

    const char* preset_name = subghz_setting_get_preset_name(setting, index);
    variable_item_set_current_value_text(item, preset_name);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
    uint8_t* preset_data = subghz_setting_get_preset_data(setting, index);
    size_t preset_data_size = subghz_setting_get_preset_data_size(setting, index);

    subghz_txrx_set_tx_power(preset_data, preset_data_size, subghz->tx_power);

    subghz_txrx_set_preset(
        subghz->txrx, preset_name, preset.frequency, preset_data, preset_data_size);
    subghz->last_settings->preset_index = index;
    FURI_LOG_I(TAG, "set_preset: done");
}

static void subghz_scene_receiver_config_set_hopping(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item); // 0=OFF, 1=ON
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    VariableItem* frequency_item = (VariableItem*)scene_manager_get_scene_state(
        subghz->scene_manager, SubGhzSceneReceiverConfig);
    FURI_LOG_I(TAG, "set_hopping: enter, index=%d", (int)index);

    variable_item_set_current_value_text(item, combobox_text[index]);

    if(index == 0) {
        char text_buf[10] = {0};
        uint32_t frequency = subghz_setting_get_default_frequency(setting);
        SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);

        snprintf(
            text_buf,
            sizeof(text_buf),
            "%lu.%02lu",
            frequency / 1000000,
            (frequency % 1000000) / 10000);
        variable_item_set_current_value_text(frequency_item, text_buf);

        //Edit TX power, if necessary.
        subghz_txrx_set_tx_power(preset.data, preset.data_size, subghz->tx_power);

        // Maybe better add one more function with only with the frequency argument?
        subghz_txrx_set_preset(
            subghz->txrx,
            furi_string_get_cstr(preset.name),
            frequency,
            preset.data,
            preset.data_size);
        variable_item_set_current_value_index(
            frequency_item, subghz_setting_get_frequency_default_index(setting));
    } else {
        variable_item_set_current_value_text(frequency_item, " -----");
        variable_item_set_current_value_index(
            frequency_item, subghz_setting_get_frequency_default_index(setting));
    }
    subghz->last_settings->enable_hopping = index != 0;
    subghz->last_settings->hopping_threshold = SUBGHZ_GARAGE_HOPPING_RSSI_THRESHOLD;
    subghz_txrx_hopper_set_state(
        subghz->txrx, index != 0 ? SubGhzHopperStateRunning : SubGhzHopperStateOFF);
    FURI_LOG_I(TAG, "set_hopping: done");
}

static void subghz_scene_receiver_config_set_speaker(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    FURI_LOG_I(TAG, "set_speaker: enter, index=%d", (int)index);

    variable_item_set_current_value_text(item, combobox_text[index]);
    subghz_txrx_speaker_set_state(subghz->txrx, speaker_value[index]);
    FURI_LOG_I(TAG, "set_speaker: done");
}

static void subghz_scene_receiver_config_set_bin_raw(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    FURI_LOG_I(TAG, "set_bin_raw: enter, index=%d", (int)index);

    variable_item_set_current_value_text(item, combobox_text[index]);
    subghz->filter = bin_raw_value[index];
    subghz_txrx_receiver_set_filter(subghz->txrx, subghz->filter);

    // We can set here, but during subghz_garage_last_settings_save filter was changed to ignore BinRAW
    subghz->last_settings->filter = subghz->filter;
    FURI_LOG_I(TAG, "set_bin_raw: done");
}

static void subghz_scene_receiver_config_set_raw_threshold_rssi(VariableItem* item) {
    SubGhz* subghz = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    FURI_LOG_I(TAG, "set_raw_threshold_rssi: enter, index=%d", (int)index);

    variable_item_set_current_value_text(item, raw_threshold_rssi_text[index]);
    subghz_threshold_rssi_set(subghz->threshold_rssi, raw_threshold_rssi_value[index]);

    subghz->last_settings->rssi = raw_threshold_rssi_value[index];
    FURI_LOG_I(TAG, "set_raw_threshold_rssi: done");
}

void subghz_scene_receiver_config_on_enter(void* context) {
    SubGhz* subghz = context;
    FURI_LOG_I(TAG, "on_enter: enter, free heap %zu", memmgr_get_free_heap());
    subghz_ensure_variable_item_list(subghz);
    VariableItem* item;
    uint8_t value_index;
    SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
    SubGhzRadioPreset preset = subghz_txrx_get_preset(subghz->txrx);
    FURI_LOG_I(TAG, "on_enter: adding Frequency item");

    item = variable_item_list_add(
        subghz->variable_item_list,
        "Frequency",
        subghz_setting_get_frequency_count(setting),
        subghz_scene_receiver_config_set_frequency,
        subghz);
    value_index = subghz_scene_receiver_config_next_frequency(preset.frequency, subghz);
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneReceiverConfig, (uint32_t)item);
    variable_item_set_current_value_index(item, value_index);
    char text_buf[10] = {0};
    uint32_t frequency = subghz_setting_get_frequency(setting, value_index);
    snprintf(
        text_buf,
        sizeof(text_buf),
        "%lu.%02lu",
        frequency / 1000000,
        (frequency % 1000000) / 10000);
    variable_item_set_current_value_text(item, text_buf);

    FURI_LOG_I(TAG, "on_enter: adding Modulation item");
    item = variable_item_list_add(
        subghz->variable_item_list,
        "Modulation",
        mod_filter_enabled_count(subghz), /* only ENABLED presets appear */
        subghz_scene_receiver_config_set_preset,
        subghz);
    /* Get actual preset index, then convert to display index */
    value_index =
        subghz_scene_receiver_config_next_preset(furi_string_get_cstr(preset.name), subghz);
    /* If current preset is now disabled, fall back to first enabled one */
    if(subghz->modulation_filter &&
       !subghz_modulation_filter_is_enabled(subghz->modulation_filter, value_index)) {
        value_index = mod_filter_nth_actual(subghz, 0);
    }
    uint8_t mod_display_idx = mod_filter_display_idx(subghz, value_index);
    variable_item_set_current_value_index(item, mod_display_idx);
    variable_item_set_current_value_text(
        item, subghz_setting_get_preset_name(setting, value_index));

    FURI_LOG_I(TAG, "on_enter: modulation added, checking hopping");
    if(scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneReadRAW) !=
       SubGhzCustomEventManagerSet) {
        // Hopping - trimmed-down On/Off toggle (see subghz_scene_receiver_config_set_hopping
        // and subghz_txrx_hopper_update() in helpers/subghz_txrx.c for what runs behind it)
        value_index = subghz_scene_receiver_config_hopper_value_index(subghz);
        item = variable_item_list_add(
            subghz->variable_item_list,
            "Hopping",
            COMBO_BOX_COUNT,
            subghz_scene_receiver_config_set_hopping,
            subghz);

        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, combobox_text[value_index]);
    }

    FURI_LOG_I(TAG, "on_enter: hopping done, checking bin raw");
    if(scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneReadRAW) !=
       SubGhzCustomEventManagerSet) {
        item = variable_item_list_add(
            subghz->variable_item_list,
            "Bin RAW",
            COMBO_BOX_COUNT,
            subghz_scene_receiver_config_set_bin_raw,
            subghz);

        value_index = value_index_uint32(subghz->filter, bin_raw_value, COMBO_BOX_COUNT);
        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, combobox_text[value_index]);
    }

    FURI_LOG_I(TAG, "on_enter: bin raw done, adding Sound item");
    // Enable speaker, will send all incoming noises and signals to speaker so you can listen how the remote sounds like :)
    item = variable_item_list_add(
        subghz->variable_item_list,
        "Sound",
        COMBO_BOX_COUNT,
        subghz_scene_receiver_config_set_speaker,
        subghz);
    value_index = value_index_uint32(
        subghz_txrx_speaker_get_state(subghz->txrx), speaker_value, COMBO_BOX_COUNT);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, combobox_text[value_index]);

    FURI_LOG_I(TAG, "on_enter: sound done, checking rssi threshold");
    if(scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneReadRAW) ==
       SubGhzCustomEventManagerSet) {
        item = variable_item_list_add(
            subghz->variable_item_list,
            "RSSI Threshold:",
            RAW_THRESHOLD_RSSI_COUNT,
            subghz_scene_receiver_config_set_raw_threshold_rssi,
            subghz);
        value_index = value_index_float(
            subghz_threshold_rssi_get(subghz->threshold_rssi),
            raw_threshold_rssi_value,
            RAW_THRESHOLD_RSSI_COUNT);
        variable_item_set_current_value_index(item, value_index);
        variable_item_set_current_value_text(item, raw_threshold_rssi_text[value_index]);
    }
    FURI_LOG_I(TAG, "on_enter: switching to variable item list view");
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdVariableItemList);
    FURI_LOG_I(TAG, "on_enter: done");
}

bool subghz_scene_receiver_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void subghz_scene_receiver_config_on_exit(void* context) {
    SubGhz* subghz = context;
    FURI_LOG_I(TAG, "on_exit: enter, preset freq=%lu",
        (unsigned long)subghz_txrx_get_preset(subghz->txrx).frequency);
    variable_item_list_set_selected_item(subghz->variable_item_list, 0);
    variable_item_list_reset(subghz->variable_item_list);

    FURI_LOG_I(TAG, "on_exit: saving settings");
    subghz_save_all(subghz);
    subghz_config_sync_preset_to_fa(subghz->last_settings->preset_index);
    scene_manager_set_scene_state(
        subghz->scene_manager, SubGhzSceneReadRAW, SubGhzCustomEventManagerNoSet);
    FURI_LOG_I(TAG, "on_exit: done");
}
