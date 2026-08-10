#include "../subghz_i.h"
#include "../helpers/subghz_custom_event.h"
#include <lib/toolbox/path.h>
#include <storage/storage.h>
#include <stdlib.h>

#define TAG "SubGhzCustomFreq"

#define SUBGHZ_SETTING_USER_PATH  EXT_PATH("subghz/assets/setting_user")
#define SUBGHZ_SETTING_FILE_TYPE_ "Flipper SubGhz Setting File"
#define SUBGHZ_SETTING_FILE_VER_  1

#define CUSTOM_FREQ_MAX_ENTRIES 32

enum {
    CustomFreqIdxAddStatic = 0,
    CustomFreqIdxAddHopper,
    CustomFreqIdxToggleStandard,
    CustomFreqIdxLockedInfo,
    CustomFreqIdxRemoveBase,
};

typedef struct {
    uint32_t freq[CUSTOM_FREQ_MAX_ENTRIES];
    size_t count;
} CustomFreqList;

static bool subghz_scene_custom_freq_has_advanced(FlipperFormat* fff) {
    return flipper_format_key_exist(fff, "Custom_preset_name") ||
           flipper_format_key_exist(fff, "Hopping_Preset");
}

static bool subghz_scene_custom_freq_read(
    Storage* storage,
    bool* add_standard,
    uint32_t* default_frequency,
    bool* has_default_frequency,
    bool* locked,
    CustomFreqList* statics,
    CustomFreqList* hoppers) {
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_ver;
    bool file_exists = false;

    *add_standard = true;
    *has_default_frequency = false;
    *locked = false;
    statics->count = 0;
    hoppers->count = 0;

    do {
        if(!flipper_format_file_open_existing(fff, SUBGHZ_SETTING_USER_PATH)) break;
        file_exists = true;
        if(!flipper_format_read_header(fff, temp_str, &temp_ver)) break;
        if(strcmp(furi_string_get_cstr(temp_str), SUBGHZ_SETTING_FILE_TYPE_) ||
           temp_ver != SUBGHZ_SETTING_FILE_VER_) {
            break;
        }

        *locked = subghz_scene_custom_freq_has_advanced(fff);

        flipper_format_rewind(fff);
        flipper_format_read_bool(fff, "Add_standard_frequencies", add_standard, 1);

        flipper_format_rewind(fff);
        *has_default_frequency =
            flipper_format_read_uint32(fff, "Default_frequency", default_frequency, 1);

        flipper_format_rewind(fff);
        uint32_t val;
        while(statics->count < CUSTOM_FREQ_MAX_ENTRIES &&
              flipper_format_read_uint32(fff, "Frequency", &val, 1)) {
            statics->freq[statics->count++] = val;
        }

        flipper_format_rewind(fff);
        while(hoppers->count < CUSTOM_FREQ_MAX_ENTRIES &&
              flipper_format_read_uint32(fff, "Hopper_frequency", &val, 1)) {
            hoppers->freq[hoppers->count++] = val;
        }
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(fff);
    return file_exists;
}

/* Full-rewrite of the managed keys only. Only called when `locked` is false,
 * i.e. the file has no Custom_preset_name/Hopping_Preset entries to risk
 * losing. */
static bool subghz_scene_custom_freq_write(
    Storage* storage,
    bool add_standard,
    uint32_t default_frequency,
    bool has_default_frequency,
    CustomFreqList* statics,
    CustomFreqList* hoppers) {
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool result = false;

    do {
        if(!flipper_format_file_open_always(fff, SUBGHZ_SETTING_USER_PATH)) break;
        if(!flipper_format_write_header_cstr(fff, SUBGHZ_SETTING_FILE_TYPE_, SUBGHZ_SETTING_FILE_VER_))
            break;
        if(!flipper_format_write_bool(fff, "Add_standard_frequencies", &add_standard, 1)) break;
        if(has_default_frequency) {
            if(!flipper_format_write_uint32(fff, "Default_frequency", &default_frequency, 1))
                break;
        }
        bool ok = true;
        for(size_t i = 0; i < statics->count && ok; i++) {
            ok = flipper_format_write_uint32(fff, "Frequency", &statics->freq[i], 1);
        }
        if(!ok) break;
        for(size_t i = 0; i < hoppers->count && ok; i++) {
            ok = flipper_format_write_uint32(fff, "Hopper_frequency", &hoppers->freq[i], 1);
        }
        if(!ok) break;
        result = true;
    } while(false);

    flipper_format_free(fff);
    return result;
}

/* Safe in every case: only appends, never touches existing content. */
static bool subghz_scene_custom_freq_append(Storage* storage, const char* key, uint32_t value) {
    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool result = false;

    if(!storage_file_exists(storage, SUBGHZ_SETTING_USER_PATH)) {
        if(flipper_format_file_open_always(fff, SUBGHZ_SETTING_USER_PATH)) {
            flipper_format_write_header_cstr(
                fff, SUBGHZ_SETTING_FILE_TYPE_, SUBGHZ_SETTING_FILE_VER_);
            bool add_standard = true;
            flipper_format_write_bool(fff, "Add_standard_frequencies", &add_standard, 1);
            flipper_format_file_close(fff);
        }
    }

    if(flipper_format_file_open_append(fff, SUBGHZ_SETTING_USER_PATH)) {
        result = flipper_format_write_uint32(fff, key, &value, 1);
        flipper_format_file_close(fff);
    }

    flipper_format_free(fff);
    return result;
}

static void subghz_scene_custom_freq_submenu_callback(void* context, uint32_t index) {
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, index);
}

static void subghz_scene_custom_freq_rebuild(void* context) {
    SubGhz* subghz = context;
    Submenu* submenu = subghz->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, "Custom Frequencies");

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool add_standard, has_default, locked;
    uint32_t default_frequency;
    CustomFreqList statics, hoppers;
    subghz_scene_custom_freq_read(
        storage, &add_standard, &default_frequency, &has_default, &locked, &statics, &hoppers);
    furi_record_close(RECORD_STORAGE);

    submenu_add_item(
        submenu,
        "Add Static Frequency",
        CustomFreqIdxAddStatic,
        subghz_scene_custom_freq_submenu_callback,
        subghz);
    submenu_add_item(
        submenu,
        "Add Hopper Frequency",
        CustomFreqIdxAddHopper,
        subghz_scene_custom_freq_submenu_callback,
        subghz);

    if(locked) {
        submenu_add_item(
            submenu,
            "(Custom presets in file - edit via SD)",
            CustomFreqIdxLockedInfo,
            subghz_scene_custom_freq_submenu_callback,
            subghz);
    } else {
        submenu_add_item(
            submenu,
            add_standard ? "Standard Freqs: On" : "Standard Freqs: Off",
            CustomFreqIdxToggleStandard,
            subghz_scene_custom_freq_submenu_callback,
            subghz);

        char label[40];
        for(size_t i = 0; i < statics.count; i++) {
            snprintf(
                label,
                sizeof(label),
                "Remove %lu.%03lu MHz (S)",
                statics.freq[i] / 1000000,
                (statics.freq[i] / 1000) % 1000);
            submenu_add_item(
                submenu,
                label,
                CustomFreqIdxRemoveBase + i,
                subghz_scene_custom_freq_submenu_callback,
                subghz);
        }
        for(size_t i = 0; i < hoppers.count; i++) {
            snprintf(
                label,
                sizeof(label),
                "Remove %lu.%03lu MHz (H)",
                hoppers.freq[i] / 1000000,
                (hoppers.freq[i] / 1000) % 1000);
            submenu_add_item(
                submenu,
                label,
                CustomFreqIdxRemoveBase + statics.count + i,
                subghz_scene_custom_freq_submenu_callback,
                subghz);
        }
    }
}

void subghz_scene_custom_freq_on_enter(void* context) {
    subghz_scene_custom_freq_rebuild(context);
    SubGhz* subghz = context;
    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdMenu);
}

static void subghz_scene_custom_freq_text_input_callback(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    view_dispatcher_send_custom_event(subghz->view_dispatcher, SubGhzCustomEventSceneCustomFreqAdd);
}

bool subghz_scene_custom_freq_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;

        if(idx == CustomFreqIdxAddStatic || idx == CustomFreqIdxAddHopper) {
            scene_manager_set_scene_state(
                subghz->scene_manager, SubGhzSceneCustomFreq, idx == CustomFreqIdxAddHopper);
            subghz->file_name_tmp[0] = '\0';
            text_input_set_header_text(subghz->text_input, "Freq. in MHz (e.g. 433.920)");
            text_input_set_result_callback(
                subghz->text_input,
                subghz_scene_custom_freq_text_input_callback,
                subghz,
                subghz->file_name_tmp,
                16,
                true);
            view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdTextInput);
            return true;
        } else if(idx == SubGhzCustomEventSceneCustomFreqAdd) {
            float mhz = strtof(subghz->file_name_tmp, NULL);
            uint32_t hz = (uint32_t)(mhz * 1000000.0f + 0.5f);
            bool is_hopper =
                scene_manager_get_scene_state(subghz->scene_manager, SubGhzSceneCustomFreq);

            if(hz > 0 && furi_hal_subghz_is_frequency_valid(hz)) {
                Storage* storage = furi_record_open(RECORD_STORAGE);
                subghz_scene_custom_freq_append(
                    storage, is_hopper ? "Hopper_frequency" : "Frequency", hz);
                furi_record_close(RECORD_STORAGE);
                furi_string_set(subghz->error_str, "Frequency added.\nRestart Sub-GHz to apply.");
            } else {
                furi_string_set(subghz->error_str, "Invalid or unsupported\nfrequency.");
            }
            scene_manager_next_scene(subghz->scene_manager, SubGhzSceneShowErrorSub);
            return true;
        } else if(idx == CustomFreqIdxToggleStandard) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            bool add_standard, has_default, locked;
            uint32_t default_frequency;
            CustomFreqList statics, hoppers;
            subghz_scene_custom_freq_read(
                storage,
                &add_standard,
                &default_frequency,
                &has_default,
                &locked,
                &statics,
                &hoppers);
            if(!locked) {
                subghz_scene_custom_freq_write(
                    storage, !add_standard, default_frequency, has_default, &statics, &hoppers);
            }
            furi_record_close(RECORD_STORAGE);
            subghz_scene_custom_freq_rebuild(subghz);
            return true;
        } else if(idx == CustomFreqIdxLockedInfo) {
            return true;
        } else if(idx >= CustomFreqIdxRemoveBase) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            bool add_standard, has_default, locked;
            uint32_t default_frequency;
            CustomFreqList statics, hoppers;
            subghz_scene_custom_freq_read(
                storage,
                &add_standard,
                &default_frequency,
                &has_default,
                &locked,
                &statics,
                &hoppers);
            if(!locked) {
                size_t remove_idx = idx - CustomFreqIdxRemoveBase;
                if(remove_idx < statics.count) {
                    for(size_t i = remove_idx; i + 1 < statics.count; i++) {
                        statics.freq[i] = statics.freq[i + 1];
                    }
                    statics.count--;
                } else {
                    remove_idx -= statics.count;
                    if(remove_idx < hoppers.count) {
                        for(size_t i = remove_idx; i + 1 < hoppers.count; i++) {
                            hoppers.freq[i] = hoppers.freq[i + 1];
                        }
                        hoppers.count--;
                    }
                }
                subghz_scene_custom_freq_write(
                    storage, add_standard, default_frequency, has_default, &statics, &hoppers);
            }
            furi_record_close(RECORD_STORAGE);
            subghz_scene_custom_freq_rebuild(subghz);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(subghz->scene_manager);
        return true;
    }

    return false;
}

void subghz_scene_custom_freq_on_exit(void* context) {
    SubGhz* subghz = context;
    submenu_reset(subghz->submenu);
    text_input_reset(subghz->text_input);
}
