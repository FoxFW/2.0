#include "tpms_ford.h"
#include <lib/toolbox/manchester_decoder.h>
#include "../helpers/fox_tpms_type_compat.h"
#include "../helpers/fox_tpms_protocol_flag_compat.h"

#define TAG "TPMSFord"

#define FORD_PREAMBLE 0x5556U
#define FORD_PREAMBLE_BITS 16
#define FORD_DATA_BITS 64

static const SubGhzBlockConst tpms_protocol_ford_const = {
    .te_short = 52,
    .te_long = 104,
    .te_delta = 20,
    .min_count_bit_for_found = FORD_DATA_BITS,
};

typedef enum {
    FordDecoderStepReset = 0,
    FordDecoderStepFindPreamble,
    FordDecoderStepData,
} FordDecoderStep;

typedef struct {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    TPMSBlockGeneric generic;
    ManchesterState manchester_saved_state;
    uint16_t header_shift;
} TPMSProtocolDecoderFord;

const SubGhzProtocolDecoder tpms_protocol_ford_decoder = {
    .alloc = tpms_protocol_decoder_ford_alloc,
    .free = tpms_protocol_decoder_ford_free,
    .feed = tpms_protocol_decoder_ford_feed,
    .reset = tpms_protocol_decoder_ford_reset,
    .get_hash_data = tpms_protocol_decoder_ford_get_hash_data,
    .serialize = tpms_protocol_decoder_ford_serialize,
    .deserialize = tpms_protocol_decoder_ford_deserialize,
    .get_string = tpms_protocol_decoder_ford_get_string,
};

const SubGhzProtocolEncoder tpms_protocol_ford_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol tpms_protocol_ford = {
    .name = TPMS_PROTOCOL_FORD_NAME,
    .type = FOX_TPMS_PROTOCOL_TYPE,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_315 | SubGhzProtocolFlag_FM |
            SubGhzProtocolFlag_Decodable | FOX_TPMS_FLAG_SENSORS |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Load,

    .decoder = &tpms_protocol_ford_decoder,
    .encoder = &tpms_protocol_ford_encoder,
};

void* tpms_protocol_decoder_ford_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    TPMSProtocolDecoderFord* instance = malloc(sizeof(TPMSProtocolDecoderFord));
    instance->base.protocol = &tpms_protocol_ford;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void tpms_protocol_decoder_ford_free(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;
    free(instance);
}

void tpms_protocol_decoder_ford_reset(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;
    instance->decoder.parser_step = FordDecoderStepReset;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
    instance->header_shift = 0;
}

static ManchesterEvent ford_level_and_duration_to_event(bool level, uint32_t duration) {
    bool is_long = false;
    if(DURATION_DIFF(duration, tpms_protocol_ford_const.te_long) <
       tpms_protocol_ford_const.te_delta * 2) {
        is_long = true;
    } else if(
        DURATION_DIFF(duration, tpms_protocol_ford_const.te_short) <
        tpms_protocol_ford_const.te_delta) {
        is_long = false;
    } else {
        return ManchesterEventReset;
    }
    if(level)
        return is_long ? ManchesterEventLongHigh : ManchesterEventShortHigh;
    else
        return is_long ? ManchesterEventLongLow : ManchesterEventShortLow;
}

static bool tpms_protocol_ford_check_checksum(TPMSProtocolDecoderFord* instance) {
    uint8_t b[8];
    for(int i = 0; i < 8; i++) {
        b[i] = (instance->decoder.decode_data >> (8 * (7 - i))) & 0xFF;
    }
    uint16_t sum = 0;
    for(int i = 0; i < 7; i++) sum += b[i];
    return ((uint8_t)(sum & 0xFF)) == b[7];
}

static void tpms_protocol_ford_analyze(TPMSBlockGeneric* g) {
    uint8_t b[8];
    for(int i = 0; i < 8; i++) {
        b[i] = (g->data >> (8 * (7 - i))) & 0xFF;
    }
    g->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];

    uint16_t psi_bits = ((uint16_t)(b[6] & 0x20) << 3) | b[4];
    float psi = (float)psi_bits * 0.25f;
    g->pressure = psi * 0.06895f;

    if((b[5] & 0x80) == 0) {
        g->temperature = (float)((int)(b[5] & 0x7F) - 56);
    } else {

        g->temperature = -100.0f;
    }
    g->battery_low = TPMS_NO_BATT;
}

void tpms_protocol_decoder_ford_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;

    ManchesterEvent event = ford_level_and_duration_to_event(level, duration);
    if(event == ManchesterEventReset) {
        instance->decoder.parser_step = FordDecoderStepReset;
        instance->decoder.decode_data = 0;
        instance->decoder.decode_count_bit = 0;
        instance->header_shift = 0;
        instance->manchester_saved_state = ManchesterStateStart1;
        return;
    }

    bool bit = false;
    bool have_bit = manchester_advance(
        instance->manchester_saved_state, event, &instance->manchester_saved_state, &bit);
    if(!have_bit) return;

    switch(instance->decoder.parser_step) {
    case FordDecoderStepReset:
        instance->header_shift = 0;
        instance->manchester_saved_state = ManchesterStateStart1;
        instance->decoder.parser_step = FordDecoderStepFindPreamble;
        __attribute__((fallthrough));

    case FordDecoderStepFindPreamble:
        instance->header_shift = (uint16_t)((instance->header_shift << 1) | (bit ? 1 : 0));
        if(instance->header_shift == FORD_PREAMBLE) {
            instance->decoder.parser_step = FordDecoderStepData;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        }
        break;

    case FordDecoderStepData:
        subghz_protocol_blocks_add_bit(&instance->decoder, bit ? 1 : 0);
        if(instance->decoder.decode_count_bit ==
           tpms_protocol_ford_const.min_count_bit_for_found) {
            if(tpms_protocol_ford_check_checksum(instance)) {
                instance->generic.data = instance->decoder.decode_data;
                instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                tpms_protocol_ford_analyze(&instance->generic);
                if(instance->base.callback)
                    instance->base.callback(&instance->base, instance->base.context);
            }
            instance->decoder.parser_step = FordDecoderStepReset;
        }
        break;
    }
}

uint8_t tpms_protocol_decoder_ford_get_hash_data(void* context) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus tpms_protocol_decoder_ford_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;
    return tpms_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    tpms_protocol_decoder_ford_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;
    return tpms_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        tpms_protocol_ford_const.min_count_bit_for_found);
}

void tpms_protocol_ford_pack(TPMSBlockGeneric* generic) {
    furi_assert(generic);

    int32_t psi_bits = (int32_t)((generic->pressure / 0.06895f) / 0.25f + 0.5f);
    if(psi_bits < 0) psi_bits = 0;
    if(psi_bits > 0x1FF) psi_bits = 0x1FF;

    int32_t temperature_raw = (int32_t)(generic->temperature + 56.0f +
                                         (generic->temperature >= 0 ? 0.5f : -0.5f));
    if(temperature_raw < 0) temperature_raw = 0;
    if(temperature_raw > 0x7F) temperature_raw = 0x7F;

    uint8_t b[8];
    b[0] = (generic->id >> 24) & 0xFF;
    b[1] = (generic->id >> 16) & 0xFF;
    b[2] = (generic->id >> 8) & 0xFF;
    b[3] = (generic->id >> 0) & 0xFF;
    b[4] = (uint8_t)(psi_bits & 0xFF);

    b[5] = (uint8_t)(temperature_raw & 0x7F);

    b[6] = (uint8_t)(((psi_bits >> 8) & 0x1) << 5);

    uint16_t sum = 0;
    for(int i = 0; i < 7; i++) sum += b[i];
    b[7] = (uint8_t)(sum & 0xFF);

    uint64_t data = 0;
    for(int i = 0; i < 8; i++) {
        data = (data << 8) | b[i];
    }
    generic->data = data;
    generic->data_count_bit = FORD_DATA_BITS;
}

void tpms_protocol_decoder_ford_get_string(void* context, FuriString* output) {
    furi_assert(context);
    TPMSProtocolDecoderFord* instance = context;
    furi_string_printf(
        output,
        "%s\r\n"
        "Id:0x%08lX\r\n"
        "Temp:%2.0f C Bar:%2.1f",
        instance->generic.protocol_name,
        instance->generic.id,
        (double)instance->generic.temperature,
        (double)instance->generic.pressure);
}
