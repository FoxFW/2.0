#include "subratt_device.h"

#include <stdlib.h>
#include <storage/storage.h>
#include <lib/flipper_format/flipper_format_i.h>
#include <toolbox/stream/stream.h>
#include "protocols/protocol_items.h"

#define TAG "SubRattDevice"

SubRattDevice* subratt_device_alloc(const SubGhzDevice* radio_device) {

    SubRattDevice* instance = calloc(1, sizeof(SubRattDevice));

    instance->current_step = 0;

    instance->protocol_info = NULL;
    instance->file_protocol_info = NULL;
    instance->decoder_result = NULL;
    instance->receiver = NULL;
    instance->environment = subghz_environment_alloc();

    subghz_environment_set_protocol_registry(
        instance->environment, (void*)&subratt_subghz_protocol_registry);

    instance->radio_device = radio_device;

    instance->replay_file_path = furi_string_alloc();

    subratt_device_attack_set_default_values(instance, SubRattAttackCAME12bit433);

    return instance;
}

void subratt_device_free(SubRattDevice* instance) {
    furi_assert(instance);

    instance->decoder_result = NULL;

    if(instance->receiver != NULL) {
        subghz_receiver_free(instance->receiver);
        instance->receiver = NULL;
    }

    subghz_environment_free(instance->environment);
    instance->environment = NULL;

    subratt_device_free_protocol_info(instance);

    if(instance->replay_file_path != NULL) {
        furi_string_free(instance->replay_file_path);
        instance->replay_file_path = NULL;
    }

    free(instance);
}

uint64_t subratt_device_add_step(SubRattDevice* instance, int8_t step) {
    if(step > 0) {
        if((instance->current_step + step) - instance->max_value == 1) {
            instance->current_step = 0x00;
        } else {
            uint64_t value = instance->current_step + step;
            if(value == instance->max_value) {
                instance->current_step = value;
            } else {
                instance->current_step = value % instance->max_value;
            }
        }
    } else {
        if(instance->current_step + step == 0) {
            instance->current_step = 0x00;
        } else if(instance->current_step == 0) {
            instance->current_step = instance->max_value;
        } else {
            uint64_t value = ((instance->current_step + step) + instance->max_value);
            if(value == instance->max_value) {
                instance->current_step = value;
            } else {
                instance->current_step = value % instance->max_value;
            }
        }
    }

    return instance->current_step;
}

bool subratt_device_save_file(SubRattDevice* instance, const char* dev_file_name) {
    furi_assert(instance);

#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_device_save_file: %s", dev_file_name);
    FURI_LOG_D(TAG, "opencode: %d", instance->opencode);
#endif

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool result = false;
    do {
        if(!flipper_format_file_open_always(file, dev_file_name)) {
            FURI_LOG_E(TAG, "Failed to open file: %s", dev_file_name);
            break;
        }
        Stream* stream = flipper_format_get_raw_stream(file);
        if(instance->attack == SubRattAttackLoadFile) {
            subratt_protocol_file_generate_file(
                stream,
                instance->file_protocol_info->frequency,
                instance->file_protocol_info->preset,
                instance->file_protocol_info->file,
                instance->current_step,
                instance->file_protocol_info->bits,
                instance->file_protocol_info->te,
                instance->bit_index,
                instance->key_from_file,
                instance->two_bytes);
        } else {
            subratt_protocol_default_generate_file(
                stream,
                instance->protocol_info->frequency,
                instance->protocol_info->preset,
                instance->protocol_info->file,
                instance->current_step,
                instance->protocol_info->bits,
                instance->protocol_info->te,
                instance->opencode);
        }

        result = true;
    } while(false);

    if(!result) {
        FURI_LOG_E(TAG, "subratt_device_save_file failed!");
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

SubRattFileResult subratt_device_attack_set(
    SubRattDevice* instance,
    SubRattAttacks type,
    uint8_t extra_repeats) {
    furi_assert(instance);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_device_attack_set: %d, extra_repeats: %d", type, extra_repeats);
#endif
    subratt_device_attack_set_default_values(instance, type);

    if(type != SubRattAttackLoadFile) {
        subratt_device_free_protocol_info(instance);
        instance->protocol_info = subratt_protocol(type);
    }

    instance->extra_repeats = extra_repeats;

    instance->receiver = subghz_receiver_alloc_init(instance->environment);
    subghz_receiver_set_filter(instance->receiver, SubGhzProtocolFlag_Decodable);

    uint8_t protocol_check_result = SubRattFileResultProtocolNotFound;
#ifdef FURI_DEBUG
    uint8_t bits;
    uint32_t te;
    uint8_t repeat;
    FuriHalSubGhzPreset preset;
    SubRattFileProtocol file;
#endif
    if(type != SubRattAttackLoadFile) {
        instance->decoder_result = subghz_receiver_search_decoder_base_by_name(
            instance->receiver, subratt_protocol_file(instance->protocol_info->file));

        if(!instance->decoder_result ||
           instance->decoder_result->protocol->type == SubGhzProtocolTypeDynamic) {
            FURI_LOG_E(TAG, "Can't load SubGhzProtocolDecoderBase in phase non-file decoder set");
        } else {
            protocol_check_result = SubRattFileResultOk;

            instance->max_value = subratt_protocol_calc_max_value(
                instance->attack, instance->protocol_info->bits, instance->two_bytes);
        }
#ifdef FURI_DEBUG
        bits = instance->protocol_info->bits;
        te = instance->protocol_info->te;
        repeat = instance->protocol_info->repeat + instance->extra_repeats;
        preset = instance->protocol_info->preset;
        file = instance->protocol_info->file;
#endif
    } else {

        protocol_check_result = SubRattFileResultOk;

        instance->max_value = subratt_protocol_calc_max_value(
            instance->attack, instance->file_protocol_info->bits, instance->two_bytes);
#ifdef FURI_DEBUG
        bits = instance->file_protocol_info->bits;
        te = instance->file_protocol_info->te;
        repeat = instance->file_protocol_info->repeat + instance->extra_repeats;
        preset = instance->file_protocol_info->preset;
        file = instance->file_protocol_info->file;
#endif
    }

    subghz_receiver_free(instance->receiver);
    instance->receiver = NULL;
    instance->decoder_result = NULL;

    if(protocol_check_result != SubRattFileResultOk) {
        return SubRattFileResultProtocolNotFound;
    }

#ifdef FURI_DEBUG
    FURI_LOG_I(
        TAG,
        "subratt_device_attack_set: %s, bits: %d, preset: %s, file: %s, te: %ld, repeat: %d, max_value: %lld",
        subratt_protocol_name(instance->attack),
        bits,
        subratt_protocol_preset(preset),
        subratt_protocol_file(file),
        te,
        repeat,
        instance->max_value);
#endif

    return SubRattFileResultOk;
}

uint8_t subratt_device_load_from_file(SubRattDevice* instance, const char* file_path) {
    furi_assert(instance);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_device_load_from_file: %s", file_path);
#endif
    SubRattFileResult result;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);

    subratt_device_free_protocol_info(instance);
    instance->file_protocol_info = malloc(sizeof(SubRattProtocol));

    FuriString* temp_str;
    temp_str = furi_string_alloc();
    uint32_t temp_data32;

    instance->receiver = subghz_receiver_alloc_init(instance->environment);
    subghz_receiver_set_filter(instance->receiver, SubGhzProtocolFlag_Decodable);

    do {
        if(!flipper_format_file_open_existing(fff_data_file, file_path)) {
            FURI_LOG_E(TAG, "Error open file %s", file_path);
            result = SubRattFileResultErrorOpenFile;
            break;
        }
        if(!flipper_format_read_header(fff_data_file, temp_str, &temp_data32)) {
            FURI_LOG_E(TAG, error_device_missing_header);
            result = SubRattFileResultMissingOrIncorrectHeader;
            break;
        }

        if(!flipper_format_read_uint32(fff_data_file, "Frequency", &temp_data32, 1)) {
            FURI_LOG_E(TAG, error_device_incorrect_frequency);
            result = SubRattFileResultMissingOrIncorrectFrequency;
            break;
        }

        if(!subghz_devices_is_frequency_valid(instance->radio_device, temp_data32)) {
            FURI_LOG_E(TAG, "Unsupported radio device frequency");
            result = SubRattFileResultMissingOrIncorrectFrequency;
            break;
        }

        instance->file_protocol_info->frequency =
            subghz_devices_set_frequency(instance->radio_device, temp_data32);

        if(!subghz_devices_set_tx(instance->radio_device)) {
            subghz_devices_idle(instance->radio_device);
            result = SubRattFileResultFrequencyNotAllowed;
            break;
        }
        subghz_devices_idle(instance->radio_device);

        if(!flipper_format_read_string(fff_data_file, "Preset", temp_str)) {
            FURI_LOG_E(TAG, error_device_preset_fail);
            result = SubRattFileResultPresetInvalid;
            break;
        }
        instance->file_protocol_info->preset = subratt_protocol_convert_preset(temp_str);

        const char* protocol_file = NULL;

        if(!flipper_format_read_string(fff_data_file, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, error_device_missing_protocol);
            result = SubRattFileResultMissingProtocol;
            break;
        }
        instance->file_protocol_info->file = subratt_protocol_file_protocol_name(temp_str);
        protocol_file = subratt_protocol_file(instance->file_protocol_info->file);
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Protocol: %s", protocol_file);
#endif

        instance->decoder_result = subghz_receiver_search_decoder_base_by_name(
            instance->receiver, furi_string_get_cstr(temp_str));

        if((!instance->decoder_result) || (strcmp(protocol_file, "RAW") == 0) ||
           (strcmp(protocol_file, "Unknown") == 0)) {
            FURI_LOG_E(TAG, error_device_protocol_unsupported);
            result = SubRattFileResultProtocolNotSupported;
            break;
        }

        if(instance->decoder_result->protocol->type == SubGhzProtocolTypeDynamic) {
            FURI_LOG_E(TAG, "Protocol is dynamic - not supported");
            result = SubRattFileResultDynamicProtocolNotValid;
            break;
        }
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Decoder: %s", instance->decoder_result->protocol->name);
#endif

        if(!flipper_format_read_uint32(fff_data_file, "Bit", &temp_data32, 1)) {
            FURI_LOG_E(TAG, error_device_missing_bit);
            result = SubRattFileResultMissingOrIncorrectBit;
            break;
        }
        instance->file_protocol_info->bits = temp_data32;
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Bit: %d", instance->file_protocol_info->bits);
#endif

        uint8_t key_data[sizeof(uint64_t)] = {0};
        if(!flipper_format_read_hex(fff_data_file, "Key", key_data, sizeof(uint64_t))) {
            FURI_LOG_E(TAG, "Missing Key");
            result = SubRattFileResultMissingOrIncorrectKey;
            break;
        }
        uint64_t data = 0;
        for(size_t i = 0; i < sizeof(uint64_t); i++) {
            data = (data << 8) | key_data[i];
        }
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Key: %.16llX", data);
#endif
        instance->key_from_file = data;

        if(!flipper_format_read_uint32(fff_data_file, "TE", &temp_data32, 1)) {
            FURI_LOG_E(TAG, error_device_missing_te);

        } else {
            instance->file_protocol_info->te = temp_data32 != 0 ? temp_data32 : 0;
        }

        if(flipper_format_read_uint32(fff_data_file, "Repeat", &temp_data32, 1)) {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Repeat: %ld", temp_data32);
#endif
            instance->file_protocol_info->repeat = (uint8_t)temp_data32;
        } else {
#ifdef FURI_DEBUG
            FURI_LOG_D(TAG, "Repeat: 3 (default)");
#endif
            instance->file_protocol_info->repeat = 3;
        }

        result = SubRattFileResultOk;
    } while(0);

    furi_string_free(temp_str);
    flipper_format_file_close(fff_data_file);
    flipper_format_free(fff_data_file);
    furi_record_close(RECORD_STORAGE);

    subghz_receiver_free(instance->receiver);

    instance->decoder_result = NULL;
    instance->receiver = NULL;

    if(result == SubRattFileResultOk) {
#ifdef FURI_DEBUG
        FURI_LOG_D(TAG, "Loaded successfully");
#endif
    } else {
        FURI_LOG_E(TAG, "Load failed!");
        subratt_device_free_protocol_info(instance);
    }

    return result;
}

SubRattFileResult
    subratt_device_load_keylog_from_file(SubRattDevice* instance, const char* file_path) {
    furi_assert(instance);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_device_load_keylog_from_file: %s", file_path);
#endif
    SubRattFileResult result;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);

    FuriString* temp_str = furi_string_alloc();
    uint32_t temp_data32;
    uint32_t total_keys = 0;

    do {
        if(!flipper_format_file_open_existing(fff_data_file, file_path)) {
            FURI_LOG_E(TAG, "Error open file %s", file_path);
            result = SubRattFileResultErrorOpenFile;
            break;
        }
        if(!flipper_format_read_header(fff_data_file, temp_str, &temp_data32)) {
            FURI_LOG_E(TAG, error_device_missing_header);
            result = SubRattFileResultMissingOrIncorrectHeader;
            break;
        }

        if(!flipper_format_read_uint32(fff_data_file, "Frequency", &temp_data32, 1)) {
            FURI_LOG_E(TAG, error_device_incorrect_frequency);
            result = SubRattFileResultMissingOrIncorrectFrequency;
            break;
        }
        if(!subghz_devices_is_frequency_valid(instance->radio_device, temp_data32)) {
            FURI_LOG_E(TAG, "Unsupported radio device frequency");
            result = SubRattFileResultMissingOrIncorrectFrequency;
            break;
        }
        instance->replay_frequency = temp_data32;

        if(!flipper_format_read_string(fff_data_file, "Preset", temp_str)) {
            FURI_LOG_E(TAG, error_device_preset_fail);
            result = SubRattFileResultPresetInvalid;
            break;
        }
        instance->replay_preset = subratt_protocol_convert_preset(temp_str);

        if(!flipper_format_read_string(fff_data_file, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, error_device_missing_protocol);
            result = SubRattFileResultMissingProtocol;
            break;
        }
        instance->replay_file = subratt_protocol_file_protocol_name(temp_str);

        if(!flipper_format_read_uint32(fff_data_file, "Bit", &temp_data32, 1)) {
            FURI_LOG_E(TAG, error_device_missing_bit);
            result = SubRattFileResultMissingOrIncorrectBit;
            break;
        }
        instance->replay_bits = (uint8_t)temp_data32;

        if(flipper_format_read_uint32(fff_data_file, "TE", &temp_data32, 1)) {
            instance->replay_te = temp_data32;
        } else {
            instance->replay_te = 0;
        }

        if(flipper_format_read_uint32(fff_data_file, "Repeat", &temp_data32, 1)) {
            instance->replay_repeat = (uint8_t)temp_data32;
        } else {
            instance->replay_repeat = 3;
        }

        if(flipper_format_read_uint32(fff_data_file, "Opencode", &temp_data32, 1)) {
            instance->replay_opencode = (uint8_t)temp_data32;
        } else {
            instance->replay_opencode = 0;
        }

        if(flipper_format_read_uint32(fff_data_file, "IsFileAttack", &temp_data32, 1)) {
            instance->replay_is_file_attack = temp_data32 != 0;
        } else {
            instance->replay_is_file_attack = false;
        }

        if(flipper_format_read_uint32(fff_data_file, "LoadIndex", &temp_data32, 1)) {
            instance->replay_load_index = (uint8_t)temp_data32;
        } else {
            instance->replay_load_index = 0;
        }

        if(flipper_format_read_string(fff_data_file, "FileKey", temp_str)) {
            instance->replay_file_key = strtoull(furi_string_get_cstr(temp_str), NULL, 16);
        } else {
            instance->replay_file_key = 0;
        }
        if(flipper_format_read_uint32(fff_data_file, "TwoBytes", &temp_data32, 1)) {
            instance->replay_two_bytes = temp_data32 != 0;
        } else {
            instance->replay_two_bytes = false;
        }

        Stream* stream = flipper_format_get_raw_stream(fff_data_file);
        FuriString* line = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            if(furi_string_start_with_str(line, "Key:")) {
                total_keys++;
            }
        }
        furi_string_free(line);
        instance->replay_total_keys = total_keys;

        if(total_keys == 0) {
            FURI_LOG_E(TAG, "Keys log file has no keys");
            result = SubRattFileResultMissingOrIncorrectKey;
            break;
        }

        furi_string_reset(instance->replay_file_path);
        furi_string_set_str(instance->replay_file_path, file_path);

        result = SubRattFileResultOk;
    } while(0);

    furi_string_free(temp_str);
    flipper_format_file_close(fff_data_file);
    flipper_format_free(fff_data_file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

void subratt_device_attack_set_default_values(
    SubRattDevice* instance,
    SubRattAttacks default_attack) {
    furi_assert(instance);
#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "subratt_device_attack_set_default_values");
#endif
    instance->attack = default_attack;
    instance->current_step = 0x00;
    instance->bit_index = 0x00;
    instance->extra_repeats = 0;
    instance->two_bytes = false;

    if(default_attack != SubRattAttackLoadFile) {
        instance->max_value = subratt_protocol_calc_max_value(
            instance->attack, instance->bit_index, instance->two_bytes);
    }
}

void subratt_device_free_protocol_info(SubRattDevice* instance) {
    furi_assert(instance);
    instance->protocol_info = NULL;
    if(instance->file_protocol_info) {
        free(instance->file_protocol_info);
    }
    instance->file_protocol_info = NULL;
}

const char* subratt_device_error_get_desc(SubRattFileResult error_id) {
    switch(error_id) {
    case(SubRattFileResultOk):
        return error_device_ok;
    case(SubRattFileResultErrorOpenFile):
        return error_device_invalid_path;
    case(SubRattFileResultMissingOrIncorrectHeader):
        return error_device_missing_header;
    case(SubRattFileResultFrequencyNotAllowed):
        return error_device_invalid_frequency;
    case(SubRattFileResultMissingOrIncorrectFrequency):
        return error_device_incorrect_frequency;
    case(SubRattFileResultPresetInvalid):
        return error_device_preset_fail;
    case(SubRattFileResultMissingProtocol):
        return error_device_missing_protocol;
    case(SubRattFileResultProtocolNotSupported):
        return error_device_protocol_unsupported;
    case(SubRattFileResultDynamicProtocolNotValid):
        return error_device_dynamic_protocol_unsupported;
    case(SubRattFileResultProtocolNotFound):
        return error_device_protocol_not_found;
    case(SubRattFileResultMissingOrIncorrectBit):
        return error_device_missing_bit;
    case(SubRattFileResultMissingOrIncorrectKey):
        return error_device_missing_key;
    case(SubRattFileResultMissingOrIncorrectTe):
        return error_device_missing_te;
    case SubRattFileResultUnknown:
    default:
        return error_device_unknown;
    }
}
