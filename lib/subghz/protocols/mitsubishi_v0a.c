#include "mitsubishi_v0a.h"

#include "../blocks/custom_btn_i.h"

#define TAG "MitsubishiProtocolV0a"

static const SubGhzBlockConst subghz_protocol_mitsubishi_v0a_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 61,
};

#define MITSUBISHI_V0A_BIT_COUNT 61U
#define MITSUBISHI_V0A_LONG_PREAMBLE_PAIRS 0x13FU
#define MITSUBISHI_V0A_SHORT_PREAMBLE_PAIRS 0x50U

#define MITSUBISHI_V0A_PREAMBLE_MIN 72U
#define MITSUBISHI_V0A_PREAMBLE_MAX 88U
#define MITSUBISHI_V0A_SYNC_US 750U
#define MITSUBISHI_V0A_GAP_US 1500U

#define MITSUBISHI_V0A_UPLOAD_CAPACITY                                          \
    (2U + (MITSUBISHI_V0A_LONG_PREAMBLE_PAIRS * 2U) + (MITSUBISHI_V0A_BIT_COUNT * 2U) + 2U + \
     (MITSUBISHI_V0A_SHORT_PREAMBLE_PAIRS * 2U) + (MITSUBISHI_V0A_BIT_COUNT * 2U) + 2U)

struct SubGhzProtocolDecoderMitsubishiV0a {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t header_count;

    uint64_t last_data;
    bool have_last;
};

struct SubGhzProtocolEncoderMitsubishiV0a {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    MitsubishiV0aDecoderStepReset = 0,
    MitsubishiV0aDecoderStepCheckPreambula,
    MitsubishiV0aDecoderStepSaveDuration,
    MitsubishiV0aDecoderStepCheckDuration,
} MitsubishiV0aDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_mitsubishi_v0a_decoder = {
    .alloc = subghz_protocol_decoder_mitsubishi_v0a_alloc,
    .free = subghz_protocol_decoder_mitsubishi_v0a_free,

    .feed = subghz_protocol_decoder_mitsubishi_v0a_feed,
    .reset = subghz_protocol_decoder_mitsubishi_v0a_reset,

    .get_hash_data = subghz_protocol_decoder_mitsubishi_v0a_get_hash_data,
    .serialize = subghz_protocol_decoder_mitsubishi_v0a_serialize,
    .deserialize = subghz_protocol_decoder_mitsubishi_v0a_deserialize,
    .get_string = subghz_protocol_decoder_mitsubishi_v0a_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_mitsubishi_v0a_encoder = {
    .alloc = subghz_protocol_encoder_mitsubishi_v0a_alloc,
    .free = subghz_protocol_encoder_mitsubishi_v0a_free,

    .deserialize = subghz_protocol_encoder_mitsubishi_v0a_deserialize,
    .stop = subghz_protocol_encoder_mitsubishi_v0a_stop,
    .yield = subghz_protocol_encoder_mitsubishi_v0a_yield,
};

const SubGhzProtocol subghz_protocol_mitsubishi_v0a = {
    .name = MITSUBISHI_PROTOCOL_V0A_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,

    .decoder = &subghz_protocol_mitsubishi_v0a_decoder,
    .encoder = &subghz_protocol_mitsubishi_v0a_encoder,
};

static uint8_t mitsubishi_v0a_crc8(uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t j = 0; j < 8; j++) {
            if((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x7F);
            else
                crc <<= 1;
        }
    }
    return crc;
}

static uint8_t mitsubishi_v0a_calculate_crc(uint64_t data) {
    uint8_t crc_data[6];
    crc_data[0] = (data >> 48) & 0xFF;
    crc_data[1] = (data >> 40) & 0xFF;
    crc_data[2] = (data >> 32) & 0xFF;
    crc_data[3] = (data >> 24) & 0xFF;
    crc_data[4] = (data >> 16) & 0xFF;
    crc_data[5] = (data >> 8) & 0xFF;

    return mitsubishi_v0a_crc8(crc_data, 6);
}

static bool mitsubishi_v0a_verify_crc(uint64_t data) {
    uint8_t received_crc = data & 0xFF;
    uint8_t calculated_crc = mitsubishi_v0a_calculate_crc(data);
    return (received_crc == calculated_crc);
}

void* subghz_protocol_encoder_mitsubishi_v0a_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderMitsubishiV0a* instance =
        malloc(sizeof(SubGhzProtocolEncoderMitsubishiV0a));
    instance->base.protocol = &subghz_protocol_mitsubishi_v0a;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.size_upload = MITSUBISHI_V0A_UPLOAD_CAPACITY;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.repeat = 1;
    instance->encoder.is_running = false;

    return instance;
}

void subghz_protocol_encoder_mitsubishi_v0a_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderMitsubishiV0a* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

void subghz_protocol_encoder_mitsubishi_v0a_stop(void* context) {
    SubGhzProtocolEncoderMitsubishiV0a* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_mitsubishi_v0a_yield(void* context) {
    SubGhzProtocolEncoderMitsubishiV0a* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];
    if(++instance->encoder.front == instance->encoder.size_upload) {
        if(!subghz_block_generic_global.endless_tx) instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}

static void subghz_protocol_mitsubishi_v0a_check_remote_controller(SubGhzBlockGeneric* instance);

static size_t
    mitsubishi_v0a_append_short_pairs(LevelDuration* upload, size_t index, uint32_t count) {
    for(uint32_t i = count; i > 0; i--) {
        upload[index++] =
            level_duration_make(true, (uint32_t)subghz_protocol_mitsubishi_v0a_const.te_short);
        upload[index++] =
            level_duration_make(false, (uint32_t)subghz_protocol_mitsubishi_v0a_const.te_short);
    }
    return index;
}

static size_t mitsubishi_v0a_append_data_pairs(
    LevelDuration* upload,
    size_t index,
    uint64_t data,
    uint8_t bit_count) {
    for(uint8_t i = bit_count; i > 0; i--) {
        uint32_t duration = bit_read(data, i - 1) ?
                                 (uint32_t)subghz_protocol_mitsubishi_v0a_const.te_long :
                                 (uint32_t)subghz_protocol_mitsubishi_v0a_const.te_short;
        upload[index++] = level_duration_make(true, duration);
        upload[index++] = level_duration_make(false, duration);
    }
    return index;
}

static void mitsubishi_v0a_build_upload(SubGhzProtocolEncoderMitsubishiV0a* instance, uint64_t data) {
    size_t index = 0;
    instance->encoder.upload[index++] = level_duration_make(true, MITSUBISHI_V0A_SYNC_US);
    instance->encoder.upload[index++] = level_duration_make(false, MITSUBISHI_V0A_SYNC_US);
    index = mitsubishi_v0a_append_short_pairs(
        instance->encoder.upload, index, MITSUBISHI_V0A_LONG_PREAMBLE_PAIRS);
    index = mitsubishi_v0a_append_data_pairs(
        instance->encoder.upload, index, data, MITSUBISHI_V0A_BIT_COUNT);

    instance->encoder.upload[index++] = level_duration_make(true, MITSUBISHI_V0A_GAP_US);
    instance->encoder.upload[index++] = level_duration_make(false, MITSUBISHI_V0A_GAP_US);
    index = mitsubishi_v0a_append_short_pairs(
        instance->encoder.upload, index, MITSUBISHI_V0A_SHORT_PREAMBLE_PAIRS);
    index = mitsubishi_v0a_append_data_pairs(
        instance->encoder.upload, index, data, MITSUBISHI_V0A_BIT_COUNT);

    instance->encoder.upload[index++] = level_duration_make(true, MITSUBISHI_V0A_GAP_US);
    instance->encoder.upload[index++] = level_duration_make(false, MITSUBISHI_V0A_GAP_US);

    instance->encoder.front = 0;
    instance->encoder.size_upload = index;
}

SubGhzProtocolStatus
    subghz_protocol_encoder_mitsubishi_v0a_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolEncoderMitsubishiV0a* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            subghz_protocol_mitsubishi_v0a_const.min_count_bit_for_found);

        if(ret != SubGhzProtocolStatusOk) {
            break;
        }

        subghz_protocol_mitsubishi_v0a_check_remote_controller(&instance->generic);

        if(!mitsubishi_v0a_verify_crc(instance->generic.data)) {
            FURI_LOG_W(TAG, "CRC mismatch in loaded file");
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        if(subghz_custom_btn_get_original() == 0) {
            subghz_custom_btn_set_original(instance->generic.btn);
        }
        subghz_custom_btn_set_max(4);

        if(instance->generic.cnt < 0xFFFF) {
            if((instance->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
            }
        } else if(instance->generic.cnt >= 0xFFFF) {
            instance->generic.cnt = 0;
        }

        uint8_t btn = subghz_custom_btn_get() == SUBGHZ_CUSTOM_BTN_OK ?
                          subghz_custom_btn_get_original() :
                          subghz_custom_btn_get();
        instance->generic.btn = btn;

        uint64_t data = 0;
        data |= ((uint64_t)(0x0F) << 56);
        data |= ((uint64_t)(instance->generic.cnt & 0xFFFF) << 40);
        data |= ((uint64_t)(instance->generic.serial & 0x0FFFFFFF) << 12);
        data |= ((uint64_t)(btn & 0x0F) << 8);
        data |= mitsubishi_v0a_calculate_crc(data);
        instance->generic.data = data;

        mitsubishi_v0a_build_upload(instance, data);

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        uint8_t key_data[sizeof(uint64_t)] = {0};
        for(size_t i = 0; i < sizeof(uint64_t); i++) {
            key_data[sizeof(uint64_t) - i - 1] = (instance->generic.data >> i * 8) & 0xFF;
        }
        if(!flipper_format_update_hex(flipper_format, "Key", key_data, sizeof(uint64_t))) {
            FURI_LOG_E(TAG, "Unable to update Key");
            ret = SubGhzProtocolStatusErrorParserKey;
            break;
        }

        instance->encoder.is_running = true;
    } while(false);

    return ret;
}

void* subghz_protocol_decoder_mitsubishi_v0a_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderMitsubishiV0a* instance =
        malloc(sizeof(SubGhzProtocolDecoderMitsubishiV0a));
    instance->base.protocol = &subghz_protocol_mitsubishi_v0a;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->have_last = false;
    instance->last_data = 0;

    return instance;
}

void subghz_protocol_decoder_mitsubishi_v0a_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;
    free(instance);
}

void subghz_protocol_decoder_mitsubishi_v0a_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;
    instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;

}

void subghz_protocol_decoder_mitsubishi_v0a_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;

    switch(instance->decoder.parser_step) {
    case MitsubishiV0aDecoderStepReset:
        if((level) && (DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_short) <
                       subghz_protocol_mitsubishi_v0a_const.te_delta)) {
            instance->decoder.parser_step = MitsubishiV0aDecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;

    case MitsubishiV0aDecoderStepCheckPreambula:
        if(level) {
            if((DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_short) <
                subghz_protocol_mitsubishi_v0a_const.te_delta) ||
               (DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_long) <
                subghz_protocol_mitsubishi_v0a_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
            }
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_short) <
             subghz_protocol_mitsubishi_v0a_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_mitsubishi_v0a_const.te_short) <
             subghz_protocol_mitsubishi_v0a_const.te_delta)) {

            instance->header_count++;
            break;
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_long) <
             subghz_protocol_mitsubishi_v0a_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_mitsubishi_v0a_const.te_long) <
             subghz_protocol_mitsubishi_v0a_const.te_delta)) {

            if(instance->header_count > 15) {
                instance->decoder.parser_step = MitsubishiV0aDecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 1;
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
            } else {
                instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
        }
        break;

    case MitsubishiV0aDecoderStepSaveDuration:
        if(level) {
            if(duration >= (subghz_protocol_mitsubishi_v0a_const.te_long +
                             subghz_protocol_mitsubishi_v0a_const.te_delta * 2UL)) {

                instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
                if(instance->decoder.decode_count_bit ==
                   subghz_protocol_mitsubishi_v0a_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;

                    if(mitsubishi_v0a_verify_crc(instance->generic.data)) {
                        bool is_repeat_in_range = instance->have_last &&
                            (instance->last_data == instance->generic.data) &&
                            (instance->header_count >= MITSUBISHI_V0A_PREAMBLE_MIN) &&
                            (instance->header_count <= MITSUBISHI_V0A_PREAMBLE_MAX);
                        instance->last_data = instance->generic.data;
                        instance->have_last = true;

                        if(is_repeat_in_range) {
                            if(instance->base.callback)
                                instance->base.callback(&instance->base, instance->base.context);
                        }
                    } else {
                        FURI_LOG_W(TAG, "CRC verification failed, packet rejected");
                    }
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                break;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = MitsubishiV0aDecoderStepCheckDuration;
            }

        } else {
            instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
        }
        break;

    case MitsubishiV0aDecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_mitsubishi_v0a_const.te_short) <
                subghz_protocol_mitsubishi_v0a_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_short) <
                subghz_protocol_mitsubishi_v0a_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = MitsubishiV0aDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_mitsubishi_v0a_const.te_long) <
                 subghz_protocol_mitsubishi_v0a_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_mitsubishi_v0a_const.te_long) <
                 subghz_protocol_mitsubishi_v0a_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = MitsubishiV0aDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = MitsubishiV0aDecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_mitsubishi_v0a_check_remote_controller(SubGhzBlockGeneric* instance) {

    instance->serial = (uint32_t)((instance->data >> 12) & 0x0FFFFFFF);
    instance->btn = (instance->data >> 8) & 0x0F;
    instance->cnt = (instance->data >> 40) & 0xFFFF;

    if(subghz_custom_btn_get_original() == 0) {
        subghz_custom_btn_set_original(instance->btn);
    }

    subghz_custom_btn_set_max(4);
}

uint8_t subghz_protocol_decoder_mitsubishi_v0a_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_mitsubishi_v0a_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    subghz_protocol_decoder_mitsubishi_v0a_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize(&instance->generic, flipper_format);

    if(ret == SubGhzProtocolStatusOk) {
        if(instance->generic.data_count_bit <
           subghz_protocol_mitsubishi_v0a_const.min_count_bit_for_found) {
            ret = SubGhzProtocolStatusErrorParserBitCount;
        }
    }

    return ret;
}

static const char* subghz_protocol_mitsubishi_v0a_get_name_button(uint8_t btn) {
    const char* name_btn[5] = {"Unknown", "Lock", "Unlock", "Trunk", "Horn"};
    return name_btn[btn < 5 ? btn : 0];
}

void subghz_protocol_decoder_mitsubishi_v0a_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderMitsubishiV0a* instance = context;

    subghz_protocol_mitsubishi_v0a_check_remote_controller(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    uint8_t received_crc = instance->generic.data & 0xFF;
    uint8_t calculated_crc = mitsubishi_v0a_calculate_crc(instance->generic.data);
    bool crc_valid = (received_crc == calculated_crc);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%07lX  Cnt:%04lX\r\n"
        "Btn:%02X:[%s]\r\n"
        "CRC:%02X %s",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.cnt,
        instance->generic.btn,
        subghz_protocol_mitsubishi_v0a_get_name_button(instance->generic.btn),
        received_crc,
        crc_valid ? "(OK)" : "(FAIL)");
}
