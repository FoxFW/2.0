#include "schrader_gg4.h"
#include <lib/toolbox/manchester_decoder.h>
#include <lib/subghz/blocks/generic.h>
#include "../helpers/fox_tpms_type_compat.h"
#include "../helpers/fox_tpms_protocol_flag_compat.h"
#include "../helpers/fox_tpms_generic_global_compat.h"

#define TAG "Schrader"

#define PREAMBLE          0b000
#define PREAMBLE_BITS_LEN 3

static const SubGhzBlockConst tpms_protocol_schrader_gg4_const = {
    .te_short = 120,
    .te_long = 240,
    .te_delta = 55,
    .min_count_bit_for_found = 64,
};

struct TPMSProtocolDecoderSchraderGG4 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    TPMSBlockGeneric generic;

    ManchesterState manchester_saved_state;
    uint16_t header_count;
};

struct TPMSProtocolEncoderSchraderGG4 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    TPMSBlockGeneric generic;
};

typedef enum {
    SchraderGG4DecoderStepReset = 0,
    SchraderGG4DecoderStepCheckPreamble,
    SchraderGG4DecoderStepDecoderData,
    SchraderGG4DecoderStepSaveDuration,
    SchraderGG4DecoderStepCheckDuration,
} SchraderGG4DecoderStep;

const SubGhzProtocolDecoder tpms_protocol_schrader_gg4_decoder = {
    .alloc = tpms_protocol_decoder_schrader_gg4_alloc,
    .free = tpms_protocol_decoder_schrader_gg4_free,

    .feed = tpms_protocol_decoder_schrader_gg4_feed,
    .reset = tpms_protocol_decoder_schrader_gg4_reset,

    .get_hash_data = tpms_protocol_decoder_schrader_gg4_get_hash_data,
    .serialize = tpms_protocol_decoder_schrader_gg4_serialize,
    .deserialize = tpms_protocol_decoder_schrader_gg4_deserialize,
    .get_string = tpms_protocol_decoder_schrader_gg4_get_string,
};

const SubGhzProtocolEncoder tpms_protocol_schrader_gg4_encoder = {
    .alloc = tpms_protocol_encoder_schrader_gg4_alloc,
    .free = tpms_protocol_encoder_schrader_gg4_free,

    .deserialize = tpms_protocol_encoder_schrader_gg4_deserialize,
    .stop = tpms_protocol_encoder_schrader_gg4_stop,
    .yield = tpms_protocol_encoder_schrader_gg4_yield,
};

const SubGhzProtocol tpms_protocol_schrader_gg4 = {
    .name = TPMS_PROTOCOL_SCHRADER_GG4_NAME,
    .type = FOX_TPMS_PROTOCOL_TYPE,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | FOX_TPMS_FLAG_SENSORS |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Send,

    .decoder = &tpms_protocol_schrader_gg4_decoder,
    .encoder = &tpms_protocol_schrader_gg4_encoder,
};

void* tpms_protocol_decoder_schrader_gg4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    TPMSProtocolDecoderSchraderGG4* instance = malloc(sizeof(TPMSProtocolDecoderSchraderGG4));
    instance->base.protocol = &tpms_protocol_schrader_gg4;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void tpms_protocol_decoder_schrader_gg4_free(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderGG4* instance = context;
    free(instance);
}

void tpms_protocol_decoder_schrader_gg4_reset(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderGG4* instance = context;
    instance->decoder.parser_step = SchraderGG4DecoderStepReset;
}

static bool tpms_protocol_schrader_gg4_check_crc(TPMSProtocolDecoderSchraderGG4* instance) {
    uint8_t msg[] = {
        instance->decoder.decode_data >> 48,
        instance->decoder.decode_data >> 40,
        instance->decoder.decode_data >> 32,
        instance->decoder.decode_data >> 24,
        instance->decoder.decode_data >> 16,
        instance->decoder.decode_data >> 8};

    uint8_t crc = subghz_protocol_blocks_crc8(msg, 6, 0x7, 0);
    return (crc == (instance->decoder.decode_data & 0xFF));
}

static void tpms_protocol_schrader_gg4_analyze(TPMSBlockGeneric* instance) {
    instance->id = instance->data >> 24;

    instance->battery_low = TPMS_NO_BATT;

    instance->temperature = (float)(uint8_t)((instance->data >> 8) & 0xFF) - 50;
    instance->pressure = (float)(uint8_t)((instance->data >> 16) & 0xFF) * 2.5f * 0.069f;
}

static ManchesterEvent level_and_duration_to_event(bool level, uint32_t duration) {
    bool is_long = false;

    if(DURATION_DIFF(duration, tpms_protocol_schrader_gg4_const.te_long) <
       tpms_protocol_schrader_gg4_const.te_delta) {
        is_long = true;
    } else if(
        DURATION_DIFF(duration, tpms_protocol_schrader_gg4_const.te_short) <
        tpms_protocol_schrader_gg4_const.te_delta) {
        is_long = false;
    } else {
        return ManchesterEventReset;
    }

    if(level)
        return is_long ? ManchesterEventLongHigh : ManchesterEventShortHigh;
    else
        return is_long ? ManchesterEventLongLow : ManchesterEventShortLow;
}

void tpms_protocol_decoder_schrader_gg4_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    bool bit = false;
    bool have_bit = false;
    TPMSProtocolDecoderSchraderGG4* instance = context;

    if(instance->decoder.parser_step != SchraderGG4DecoderStepReset) {
        ManchesterEvent event = level_and_duration_to_event(level, duration);

        if(event == ManchesterEventReset) {
            if((instance->decoder.parser_step == SchraderGG4DecoderStepDecoderData) &&
               instance->decoder.decode_count_bit) {
                FURI_LOG_D(
                    TAG,
                    "reset accumulated %d bits: %llx",
                    instance->decoder.decode_count_bit,
                    instance->decoder.decode_data);
            }

            instance->decoder.parser_step = SchraderGG4DecoderStepReset;
        } else {
            have_bit = manchester_advance(
                instance->manchester_saved_state, event, &instance->manchester_saved_state, &bit);
            if(!have_bit) return;

            bit = !bit;
        }
    }

    switch(instance->decoder.parser_step) {
    case SchraderGG4DecoderStepReset:

        if((level) && (DURATION_DIFF(duration, tpms_protocol_schrader_gg4_const.te_long * 2) <
                       tpms_protocol_schrader_gg4_const.te_delta)) {
            instance->decoder.parser_step = SchraderGG4DecoderStepCheckPreamble;
            instance->header_count = 0;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;

            instance->manchester_saved_state = ManchesterStateStart1;
        }
        break;
    case SchraderGG4DecoderStepCheckPreamble:
        if(bit != 0) {
            instance->decoder.parser_step = SchraderGG4DecoderStepReset;
            break;
        }

        instance->header_count++;
        if(instance->header_count == PREAMBLE_BITS_LEN)
            instance->decoder.parser_step = SchraderGG4DecoderStepDecoderData;
        break;

    case SchraderGG4DecoderStepDecoderData:
        subghz_protocol_blocks_add_bit(&instance->decoder, bit);
        if(instance->decoder.decode_count_bit ==
           tpms_protocol_schrader_gg4_const.min_count_bit_for_found) {
            FURI_LOG_D(TAG, "%016llx", instance->decoder.decode_data);
            if(!tpms_protocol_schrader_gg4_check_crc(instance)) {
                FURI_LOG_D(TAG, "CRC mismatch drop");
            } else {
                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                tpms_protocol_schrader_gg4_analyze(&instance->generic);
                if(instance->base.callback)
                    instance->base.callback(&instance->base, instance->base.context);
            }
            instance->decoder.parser_step = SchraderGG4DecoderStepReset;
        }
        break;
    }
}

uint8_t tpms_protocol_decoder_schrader_gg4_get_hash_data(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderGG4* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus tpms_protocol_decoder_schrader_gg4_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderGG4* instance = context;
    return tpms_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    tpms_protocol_decoder_schrader_gg4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderGG4* instance = context;
    return tpms_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        tpms_protocol_schrader_gg4_const.min_count_bit_for_found);
}

void tpms_protocol_decoder_schrader_gg4_get_string(void* context, FuriString* output) {
    furi_assert(context);
    TPMSProtocolDecoderSchraderGG4* instance = context;
    furi_string_printf(
        output,
        "%s\r\n"
        "Id:0x%08lX\r\n"
        "Bat:%d\r\n"
        "Temp:%2.0f C Bar:%2.1f",
        instance->generic.protocol_name,
        instance->generic.id,
        instance->generic.battery_low,
        (double)instance->generic.temperature,
        (double)instance->generic.pressure);
}

#define TPMS_SCHRADER_GG4_ENCODER_UPLOAD_SIZE 200

void* tpms_protocol_encoder_schrader_gg4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    TPMSProtocolEncoderSchraderGG4* instance = malloc(sizeof(TPMSProtocolEncoderSchraderGG4));
    instance->base.protocol = &tpms_protocol_schrader_gg4;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat = 10;
    instance->encoder.size_upload = TPMS_SCHRADER_GG4_ENCODER_UPLOAD_SIZE;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_running = false;
    return instance;
}

void tpms_protocol_encoder_schrader_gg4_free(void* context) {
    furi_assert(context);
    TPMSProtocolEncoderSchraderGG4* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

void tpms_protocol_encoder_schrader_gg4_stop(void* context) {
    TPMSProtocolEncoderSchraderGG4* instance = context;
    instance->encoder.is_running = false;
}

static bool tpms_protocol_encoder_schrader_gg4_get_upload(
    TPMSProtocolEncoderSchraderGG4* instance) {
    furi_assert(instance);

    const uint32_t te_short = tpms_protocol_schrader_gg4_const.te_short;
    const uint32_t te_long = tpms_protocol_schrader_gg4_const.te_long;

    size_t index = 0;
    const size_t max = instance->encoder.size_upload;
    bool last_level = false;
    uint32_t last_duration = 0;

#define EMIT(lvl, dur)                                                                \
    do {                                                                              \
        if(last_duration != 0 && last_level == (lvl)) {                               \
            last_duration += (dur);                                                   \
        } else {                                                                      \
            if(last_duration != 0) {                                                  \
                if(index >= max) return false;                                        \
                instance->encoder.upload[index++] =                                   \
                    level_duration_make(last_level, last_duration);                   \
            }                                                                         \
            last_level = (lvl);                                                       \
            last_duration = (dur);                                                    \
        }                                                                             \
    } while(0)

    EMIT(false, te_short * 50);

    EMIT(true, te_long * 2);

    for(int i = 0; i < PREAMBLE_BITS_LEN; i++) {
        EMIT(false, te_short);
        EMIT(true, te_short);
    }

    for(int bit = instance->generic.data_count_bit - 1; bit >= 0; bit--) {
        if((instance->generic.data >> bit) & 0x1) {
            EMIT(true, te_short);
            EMIT(false, te_short);
        } else {
            EMIT(false, te_short);
            EMIT(true, te_short);
        }
    }

    if(last_duration != 0) {
        if(index >= max) return false;
        instance->encoder.upload[index++] = level_duration_make(last_level, last_duration);
    }

#undef EMIT

    instance->encoder.size_upload = index;
    return true;
}

SubGhzProtocolStatus
    tpms_protocol_encoder_schrader_gg4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    TPMSProtocolEncoderSchraderGG4* instance = context;
    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    do {
        res = tpms_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            tpms_protocol_schrader_gg4_const.min_count_bit_for_found);
        if(res != SubGhzProtocolStatusOk) {
            FURI_LOG_E(TAG, "Deserialize error");
            break;
        }

        flipper_format_read_uint32(
            flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1);

        instance->encoder.size_upload = TPMS_SCHRADER_GG4_ENCODER_UPLOAD_SIZE;
        if(!tpms_protocol_encoder_schrader_gg4_get_upload(instance)) {
            res = SubGhzProtocolStatusErrorEncoderGetUpload;
            break;
        }
        instance->encoder.front = 0;
        instance->encoder.is_running = true;
    } while(false);
    return res;
}

void tpms_protocol_schrader_gg4_pack(TPMSBlockGeneric* generic) {
    furi_assert(generic);

    uint64_t status_byte = (generic->data >> 56) & 0xFFULL;

    int32_t pressure_raw = (int32_t)(generic->pressure / (2.5f * 0.069f) + 0.5f);
    if(pressure_raw < 0) pressure_raw = 0;
    if(pressure_raw > 0xFF) pressure_raw = 0xFF;

    int32_t temperature_raw = (int32_t)(generic->temperature + 50.0f + 0.5f);
    if(temperature_raw < 0) temperature_raw = 0;
    if(temperature_raw > 0xFF) temperature_raw = 0xFF;

    uint8_t bytes[8];
    bytes[0] = (uint8_t)status_byte;
    bytes[1] = (generic->id >> 24) & 0xFF;
    bytes[2] = (generic->id >> 16) & 0xFF;
    bytes[3] = (generic->id >> 8) & 0xFF;
    bytes[4] = (generic->id >> 0) & 0xFF;
    bytes[5] = (uint8_t)pressure_raw;
    bytes[6] = (uint8_t)temperature_raw;

    bytes[7] = subghz_protocol_blocks_crc8(&bytes[1], 6, 0x7, 0);

    uint64_t data = 0;
    for(int i = 0; i < 8; i++) {
        data = (data << 8) | bytes[i];
    }
    generic->data = data;
    generic->data_count_bit = 64;
}

LevelDuration tpms_protocol_encoder_schrader_gg4_yield(void* context) {
    TPMSProtocolEncoderSchraderGG4* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        if(!FOX_TPMS_ENDLESS_TX) instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}
