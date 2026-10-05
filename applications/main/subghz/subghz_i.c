#include "subghz_i.h"

#include "assets_icons.h"
#include "subghz/types.h"
#include <furi.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <flipper_format/flipper_format.h>
#include <flipper_format/flipper_format_i.h>
#include <loader/loader.h>
#include <storage/storage.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/protocols/kia_v3_v4.h>
#include <lib/subghz/protocols/psa.h>
#include <lib/toolbox/hex.h>

#define TAG "SubGhz"

#define CC1101_EXT_PROBE_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_garage_cc1101_check.fap")
#define CC1101_PROBE_RELAUNCH_PATH EXT_PATH("subghz/.cc1101_probe_relaunch")
#define SUBGHZ_GARAGE_FAP_PATH EXT_PATH("apps/Sub-GHz/subghz_garage.fap")

static const char* subghz_protopirate_name_alias(const char* name) {
    static const struct {
        const char* pp;
        const char* sub;
    } map[] = {
        {"Chrysler V0", "Chrysler"},
        {"Ford V0", "FORD V0"},
        {"Kia V0", "KIA/HYU V0"},
        {"Kia V1", "KIA/HYU V1"},
        {"Kia V2", "KIA/HYU V2"},
        {"Kia V3", "KIA/HYU V3/V4"},
        {"Kia V4", "KIA/HYU V3/V4"},
        {"Kia V3/V4", "KIA/HYU V3/V4"},
        {"Kia V5", "KIA/HYU V5"},
        {"Kia V6", "KIA/HYU V6"},
        {"Porsche Touareg", "Porsche AG"},
        {"PSA", "PSA GROUP"},
        {"Subaru", "SUBARU"},
        {"VAG", "VAG GROUP"},
    };
    for(size_t i = 0; i < COUNT_OF(map); i++) {
        if(!strcmp(name, map[i].pp)) return map[i].sub;
    }
    return NULL;
}

static bool subghz_key_load_has_preset(SubGhzSetting* setting, const char* preset_name) {
    for(size_t i = 0; i < subghz_setting_get_preset_count(setting); i++) {
        if(!strcmp(subghz_setting_get_preset_name(setting, i), preset_name)) return true;
    }
    return false;
}

static void subghz_key_load_normalize_key(FlipperFormat* fff_data) {
    uint8_t key_data[sizeof(uint64_t)] = {0};
    flipper_format_rewind(fff_data);
    if(flipper_format_read_hex(fff_data, "Key", key_data, sizeof(key_data))) return;

    FuriString* key_str = furi_string_alloc();
    uint64_t key = 0;
    flipper_format_rewind(fff_data);
    if(flipper_format_read_string(fff_data, "Key", key_str) &&
       furi_string_size(key_str) == sizeof(uint64_t) * 2 &&
       hex_chars_to_uint64(furi_string_get_cstr(key_str), &key)) {
        for(size_t i = 0; i < sizeof(uint64_t); i++) {
            key_data[sizeof(uint64_t) - i - 1] = (key >> (i * 8)) & 0xFF;
        }
        flipper_format_rewind(fff_data);
        flipper_format_update_hex(fff_data, "Key", key_data, sizeof(key_data));
    }
    furi_string_free(key_str);
}

static void subghz_key_load_normalize_preset(FlipperFormat* fff_data) {
    FuriString* preset = furi_string_alloc();
    FuriString* preset_long = furi_string_alloc();
    flipper_format_rewind(fff_data);
    if(flipper_format_read_string(fff_data, "Preset", preset) &&
       !furi_string_start_with_str(preset, "FuriHalSubGhzPreset")) {
        subghz_block_generic_get_preset_name(furi_string_get_cstr(preset), preset_long);
        if(furi_string_cmp_str(preset_long, "FuriHalSubGhzPresetCustom")) {
            flipper_format_rewind(fff_data);
            flipper_format_update_string_cstr(
                fff_data, "Preset", furi_string_get_cstr(preset_long));
        }
    }
    furi_string_free(preset_long);
    furi_string_free(preset);
}

static void subghz_key_load_psa_add_hex(
    FlipperFormat* fff_data,
    FuriString* str,
    const char* key,
    uint32_t value,
    uint8_t bytes) {
    furi_string_reset(str);
    for(uint8_t i = bytes; i > 0; i--) {
        if(i != bytes) furi_string_cat_str(str, " ");
        furi_string_cat_printf(str, "%02X", (uint8_t)(value >> ((i - 1) * 8)));
    }
    flipper_format_write_string_cstr(fff_data, key, furi_string_get_cstr(str));
}

static void subghz_key_load_convert_psa(FlipperFormat* fff_data) {
    static const char* const keys[] = {"Serial", "Btn", "Cnt", "Type", "CRC", "Seed"};
    uint32_t value[COUNT_OF(keys)] = {0};
    bool present[COUNT_OF(keys)] = {false};

    for(size_t i = 0; i < COUNT_OF(keys); i++) {
        flipper_format_rewind(fff_data);
        present[i] = flipper_format_read_uint32(fff_data, keys[i], &value[i], 1);
    }

    const uint32_t type = value[3];
    if(present[3] && (type == 23 || type == 36)) return;

    for(size_t i = 0; i < COUNT_OF(keys); i++) {
        flipper_format_rewind(fff_data);
        flipper_format_delete_key(fff_data, keys[i]);
    }

    if(!(present[0] && present[1] && present[2] && present[3] && present[4]) ||
       (type != 0x23 && type != 0x36)) {
        return;
    }

    FuriString* str = furi_string_alloc();
    stream_seek(flipper_format_get_raw_stream(fff_data), 0, StreamOffsetFromEnd);
    subghz_key_load_psa_add_hex(fff_data, str, keys[0], value[0], 3);
    subghz_key_load_psa_add_hex(
        fff_data, str, keys[2], value[2] & 0x0FFFFFFF, (type == 0x23) ? 2 : 4);
    subghz_key_load_psa_add_hex(fff_data, str, keys[1], value[1], 1);
    subghz_key_load_psa_add_hex(fff_data, str, keys[3], type, 1);
    subghz_key_load_psa_add_hex(fff_data, str, keys[4], value[4], (type == 0x23) ? 1 : 2);
    if(present[5]) {
        subghz_key_load_psa_add_hex(fff_data, str, keys[5], value[5], 3);
    }
    furi_string_free(str);
}

void subghz_blank_transition_draw_cb(Canvas* canvas, void* ctx);

void subghz_blink_start(SubGhz* subghz) {
    furi_assert(subghz);
    notification_message(subghz->notifications, &sequence_blink_stop);
    notification_message(subghz->notifications, &sequence_blink_start_magenta);
}

void subghz_blink_stop(SubGhz* subghz) {
    furi_assert(subghz);
    notification_message(subghz->notifications, &sequence_blink_stop);
}

bool subghz_tx_start(SubGhz* subghz, FlipperFormat* flipper_format) {
    switch(subghz_txrx_tx_start(subghz->txrx, flipper_format)) {
    case SubGhzTxRxStartTxStateErrorParserOthers:
        dialog_message_show_storage_error(
            subghz->dialogs, "Error in protocol\nparameters\ndescription");
        break;
    case SubGhzTxRxStartTxStateErrorOnlyRx:
        subghz_dialog_message_freq_error(subghz, true);
        break;

    default:
        return true;
        break;
    }
    return false;
}

void subghz_dialog_message_freq_error(SubGhz* subghz, bool only_rx) {
    DialogsApp* dialogs = subghz->dialogs;
    DialogMessage* message = dialog_message_alloc();
    const char* header_text = "Frequency not supported";
    const char* message_text = "Frequency\nis outside of\nsupported range.";

    if(only_rx) {
        header_text = "Transmission is blocked";
        message_text = "Frequency\nis outside of\ndefault range.\nCheck docs.";
    }

    dialog_message_set_header(message, header_text, 63, 3, AlignCenter, AlignTop);
    dialog_message_set_text(message, message_text, 0, 17, AlignLeft, AlignTop);

    dialog_message_show(dialogs, message);
    dialog_message_free(message);
}

bool subghz_key_load(SubGhz* subghz, const char* file_path, bool show_dialog) {
    furi_assert(subghz);
    furi_assert(file_path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);
    Stream* fff_data_stream =
        flipper_format_get_raw_stream(subghz_txrx_get_fff_data(subghz->txrx));

    SubGhzLoadKeyState load_key_state = SubGhzLoadKeyStateParseErr;
    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_data32;
    bool headerless = false;

    do {
        stream_clean(fff_data_stream);
        if(!flipper_format_file_open_existing(fff_data_file, file_path)) {
            FURI_LOG_E(TAG, "Error open file %s", file_path);
            break;
        }

        if(!flipper_format_read_header(fff_data_file, temp_str, &temp_data32)) {
            if(flipper_format_key_exist(fff_data_file, "Filetype")) {
                FURI_LOG_E(TAG, "Missing or incorrect header");
                break;
            }
            headerless = true;
        } else if(((!strcmp(furi_string_get_cstr(temp_str), SUBGHZ_KEY_FILE_TYPE)) ||
                   (!strcmp(furi_string_get_cstr(temp_str), SUBGHZ_RAW_FILE_TYPE))) &&
                  temp_data32 == SUBGHZ_KEY_FILE_VERSION) {
        } else {
            FURI_LOG_E(TAG, "Type or version mismatch");
            break;
        }

        if(!flipper_format_rewind(fff_data_file) ||
           !flipper_format_read_uint32(fff_data_file, "Frequency", &temp_data32, 1)) {
            FURI_LOG_E(TAG, "Missing Frequency");
            break;
        }

        if(!subghz_txrx_radio_device_is_frequency_valid(subghz->txrx, temp_data32)) {
            FURI_LOG_E(TAG, "Frequency not supported on chosen radio module");
            load_key_state = SubGhzLoadKeyStateUnsuportedFreq;
            break;
        }

        if(!furi_hal_subghz_is_tx_allowed(temp_data32)) {
            FURI_LOG_E(TAG, "This frequency can only be used for RX");

            load_key_state = SubGhzLoadKeyStateOnlyRx;
            break;
        }

        if(!flipper_format_rewind(fff_data_file) ||
           !flipper_format_read_string(fff_data_file, "Preset", temp_str)) {
            FURI_LOG_E(TAG, "Missing Preset");
            break;
        }

        SubGhzSetting* setting = subghz_txrx_get_setting(subghz->txrx);
        const char* preset_name =
            subghz_txrx_get_preset_name(subghz->txrx, furi_string_get_cstr(temp_str));
        if(strcmp(preset_name, "")) {
            furi_string_set_str(temp_str, preset_name);
        } else if(!subghz_key_load_has_preset(setting, furi_string_get_cstr(temp_str))) {
            break;
        }

        if(!strcmp(furi_string_get_cstr(temp_str), "CUSTOM")) {

            subghz_setting_delete_custom_preset(setting, furi_string_get_cstr(temp_str));

            if(!subghz_setting_load_custom_preset(
                   setting, furi_string_get_cstr(temp_str), fff_data_file)) {
                FURI_LOG_E(TAG, "Missing Custom preset");
                break;
            }
        }
        size_t preset_index =
            subghz_setting_get_inx_preset_by_name(setting, furi_string_get_cstr(temp_str));

        uint8_t* preset_data = subghz_setting_get_preset_data(setting, preset_index);
        size_t preset_data_size = subghz_setting_get_preset_data_size(setting, preset_index);
        subghz_txrx_set_tx_power(preset_data, preset_data_size, subghz->tx_power);

        subghz_txrx_set_preset(
            subghz->txrx,
            furi_string_get_cstr(temp_str),
            temp_data32,
            preset_data,
            preset_data_size);

        if(!flipper_format_rewind(fff_data_file) ||
           !flipper_format_read_string(fff_data_file, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            break;
        }

        const char* pp_alias = subghz_protopirate_name_alias(furi_string_get_cstr(temp_str));
        if(pp_alias) {
            furi_string_set_str(temp_str, pp_alias);
        }

        if(headerless && furi_string_cmp_str(temp_str, SUBGHZ_PROTOCOL_KIA_V3_V4_NAME)) {
            FURI_LOG_E(TAG, "Missing or incorrect header");
            break;
        }

        FlipperFormat* fff_data = subghz_txrx_get_fff_data(subghz->txrx);
        if(!strcmp(furi_string_get_cstr(temp_str), "RAW")) {

            subghz->load_type_file = SubGhzLoadTypeFileRaw;
            subghz_protocol_raw_gen_fff_data(
                fff_data, file_path, subghz_txrx_radio_device_get_name(subghz->txrx));
        } else {
            subghz->load_type_file = SubGhzLoadTypeFileKey;
            if(headerless) {
                Stream* file_stream = flipper_format_get_raw_stream(fff_data_file);
                flipper_format_write_header_cstr(
                    fff_data, SUBGHZ_KEY_FILE_TYPE, SUBGHZ_KEY_FILE_VERSION);
                stream_rewind(file_stream);
                stream_copy(
                    file_stream,
                    flipper_format_get_raw_stream(fff_data),
                    stream_size(file_stream));
            } else {
                stream_copy_full(
                    flipper_format_get_raw_stream(fff_data_file),
                    flipper_format_get_raw_stream(fff_data));
            }
            subghz_key_load_normalize_preset(fff_data);
            subghz_key_load_normalize_key(fff_data);
            if(pp_alias) {
                flipper_format_rewind(fff_data);
                flipper_format_update_string_cstr(fff_data, "Protocol", pp_alias);
                if(!strcmp(pp_alias, SUBGHZ_PROTOCOL_PSA_NAME)) {
                    subghz_key_load_convert_psa(fff_data);
                }
            }
        }

        if(subghz_txrx_load_decoder_by_name_protocol(
               subghz->txrx, furi_string_get_cstr(temp_str))) {
            SubGhzProtocolStatus status = subghz_protocol_decoder_base_deserialize(
                subghz_txrx_get_decoder(subghz->txrx), fff_data);
            if(status != SubGhzProtocolStatusOk) {
                load_key_state = SubGhzLoadKeyStateProtocolDescriptionErr;
                break;
            }
        } else {
            FURI_LOG_E(TAG, "Protocol not found");
            break;
        }

        load_key_state = SubGhzLoadKeyStateOK;
    } while(0);

    furi_string_free(temp_str);
    flipper_format_free(fff_data_file);
    furi_record_close(RECORD_STORAGE);

    switch(load_key_state) {
    case SubGhzLoadKeyStateParseErr:
        if(show_dialog) {
            dialog_message_show_storage_error(subghz->dialogs, "Cannot parse\nfile");
        }
        return false;
    case SubGhzLoadKeyStateProtocolDescriptionErr:
        if(show_dialog) {
            dialog_message_show_storage_error(
                subghz->dialogs, "Error in protocol\nparameters\ndescription");
        }
        return false;

    case SubGhzLoadKeyStateUnsuportedFreq:
        if(show_dialog) {
            subghz_dialog_message_freq_error(subghz, false);
        }
        return false;

    case SubGhzLoadKeyStateOnlyRx:
        if(show_dialog) {
            subghz_dialog_message_freq_error(subghz, true);
        }
        return false;

    case SubGhzLoadKeyStateOK:
        return true;

    default:
        furi_crash("SubGhz: Unknown load_key_state.");
        return false;
    }
}

SubGhzLoadTypeFile subghz_get_load_type_file(SubGhz* subghz) {
    furi_assert(subghz);
    return subghz->load_type_file;
}

bool subghz_get_next_name_file(SubGhz* subghz, uint8_t max_len) {
    furi_assert(subghz);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* temp_str = furi_string_alloc();
    FuriString* file_name = furi_string_alloc();
    FuriString* file_path = furi_string_alloc();

    bool res = false;

    if(subghz_path_is_file(subghz->file_path)) {

        path_extract_filename(subghz->file_path, file_name, true);
        path_extract_dirname(furi_string_get_cstr(subghz->file_path), file_path);

        storage_get_next_filename(
            storage,
            furi_string_get_cstr(file_path),
            furi_string_get_cstr(file_name),
            SUBGHZ_APP_FILENAME_EXTENSION,
            file_name,
            max_len);

        furi_string_printf(
            temp_str,
            "%s/%s%s",
            furi_string_get_cstr(file_path),
            furi_string_get_cstr(file_name),
            SUBGHZ_APP_FILENAME_EXTENSION);
        furi_string_set(subghz->file_path, temp_str);
        res = true;
    }

    furi_string_free(temp_str);
    furi_string_free(file_path);
    furi_string_free(file_name);
    furi_record_close(RECORD_STORAGE);

    return res;
}

bool subghz_save_protocol_to_file(
    SubGhz* subghz,
    FlipperFormat* flipper_format,
    const char* dev_file_name) {
    furi_assert(subghz);
    furi_assert(flipper_format);
    furi_assert(dev_file_name);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* flipper_format_stream = flipper_format_get_raw_stream(flipper_format);

    bool saved = false;
    FuriString* file_dir = furi_string_alloc();

    path_extract_dirname(dev_file_name, file_dir);
    do {

        flipper_format_delete_key(flipper_format, "Repeat");

        if(!storage_simply_mkdir(storage, furi_string_get_cstr(file_dir))) {
            dialog_message_show_storage_error(subghz->dialogs, "Cannot create\nfolder");
            break;
        }

        if(!storage_simply_remove(storage, dev_file_name)) {
            break;
        }

        stream_seek(flipper_format_stream, 0, StreamOffsetFromStart);
        stream_save_to_file(flipper_format_stream, storage, dev_file_name, FSOM_CREATE_ALWAYS);

        if(storage_common_stat(storage, dev_file_name, NULL) != FSE_OK) {
            break;
        }

        saved = true;
    } while(0);
    furi_string_free(file_dir);
    furi_record_close(RECORD_STORAGE);
    return saved;
}

void subghz_save_to_file(void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    if(subghz_path_is_file(subghz->file_path)) {
        subghz_save_protocol_to_file(
            subghz,
            subghz_txrx_get_fff_data(subghz->txrx),
            furi_string_get_cstr(subghz->file_path));
    }
}

bool subghz_rename_file(SubGhz* subghz) {
    furi_assert(subghz);
    bool ret = true;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(furi_string_cmp(subghz->file_path_tmp, subghz->file_path)) {
        FS_Error fs_result = storage_common_rename(
            storage,
            furi_string_get_cstr(subghz->file_path_tmp),
            furi_string_get_cstr(subghz->file_path));

        if(fs_result != FSE_OK) {
            dialog_message_show_storage_error(subghz->dialogs, "Cannot rename\n file/directory");
            ret = false;
        }
    }
    furi_record_close(RECORD_STORAGE);

    return ret;
}

bool subghz_file_available(SubGhz* subghz) {
    furi_assert(subghz);
    bool ret = true;
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FS_Error fs_result =
        storage_common_stat(storage, furi_string_get_cstr(subghz->file_path), NULL);

    if(fs_result != FSE_OK) {
        dialog_message_show_storage_error(subghz->dialogs, "File not available\n file/directory");
        ret = false;
    }

    furi_record_close(RECORD_STORAGE);
    return ret;
}

bool subghz_delete_file(SubGhz* subghz) {
    furi_assert(subghz);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool result = storage_simply_remove(storage, furi_string_get_cstr(subghz->file_path_tmp));
    furi_record_close(RECORD_STORAGE);

    subghz_file_name_clear(subghz);

    return result;
}

void subghz_file_name_clear(SubGhz* subghz) {
    furi_assert(subghz);
    furi_string_set(subghz->file_path, SUBGHZ_APP_FOLDER);
    furi_string_reset(subghz->file_path_tmp);
}

bool subghz_path_is_file(FuriString* path) {
    return furi_string_end_with(path, SUBGHZ_APP_FILENAME_EXTENSION);
}

void subghz_lock(SubGhz* subghz) {
    furi_assert(subghz);
    subghz->lock = SubGhzLockOn;
}

void subghz_unlock(SubGhz* subghz) {
    furi_assert(subghz);
    subghz->lock = SubGhzLockOff;
}

bool subghz_is_locked(SubGhz* subghz) {
    furi_assert(subghz);
    return subghz->lock == SubGhzLockOn;
}

void subghz_rx_key_state_set(SubGhz* subghz, SubGhzRxKeyState state) {
    furi_assert(subghz);
    subghz->rx_key_state = state;
}

SubGhzRxKeyState subghz_rx_key_state_get(SubGhz* subghz) {
    furi_assert(subghz);
    return subghz->rx_key_state;
}

void subghz_launch_garage_via_probe(SubGhz* subghz, const char* args) {
    if(!subghz->blank_transition_viewport) {
        subghz->blank_transition_viewport = view_port_alloc();
        view_port_draw_callback_set(
            subghz->blank_transition_viewport, subghz_blank_transition_draw_cb, NULL);
        gui_add_view_port(subghz->gui, subghz->blank_transition_viewport, GuiLayerFullscreen);
        view_port_update(subghz->blank_transition_viewport);
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool probe_installed = storage_file_exists(storage, CC1101_EXT_PROBE_FAP_PATH);

    Loader* loader = furi_record_open(RECORD_LOADER);
    if(probe_installed) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, CC1101_PROBE_RELAUNCH_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(f, args, strlen(args));
        }
        storage_file_close(f);
        storage_file_free(f);
        loader_enqueue_launch(
            loader, CC1101_EXT_PROBE_FAP_PATH, NULL, LoaderDeferredLaunchFlagNone);
    } else {
        FURI_LOG_W(TAG, "CC1101 probe .fap not found, launching Garage directly");
        loader_enqueue_launch(loader, SUBGHZ_GARAGE_FAP_PATH, args, LoaderDeferredLaunchFlagNone);
    }
    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_STORAGE);

    scene_manager_stop(subghz->scene_manager);
    view_dispatcher_stop(subghz->view_dispatcher);
}
