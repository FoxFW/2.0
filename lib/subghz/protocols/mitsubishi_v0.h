#pragma once

#include <furi.h>
#include <lib/subghz/protocols/base.h>
#include <lib/subghz/types.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <flipper_format/flipper_format.h>

#define MITSUBISHI_PROTOCOL_V0_NAME "Mitsubishi V0"

typedef struct SubGhzProtocolDecoderMitsubishiV0 SubGhzProtocolDecoderMitsubishiV0;
typedef struct SubGhzProtocolEncoderMitsubishiV0 SubGhzProtocolEncoderMitsubishiV0;

extern const SubGhzProtocol subghz_protocol_mitsubishi_v0;

void* subghz_protocol_decoder_mitsubishi_v0_alloc(SubGhzEnvironment* environment);

void subghz_protocol_decoder_mitsubishi_v0_free(void* context);

void subghz_protocol_decoder_mitsubishi_v0_reset(void* context);

void subghz_protocol_decoder_mitsubishi_v0_feed(void* context, bool level, uint32_t duration);

uint8_t subghz_protocol_decoder_mitsubishi_v0_get_hash_data(void* context);

SubGhzProtocolStatus subghz_protocol_decoder_mitsubishi_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    subghz_protocol_decoder_mitsubishi_v0_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_decoder_mitsubishi_v0_get_string(void* context, FuriString* output);

void* subghz_protocol_encoder_mitsubishi_v0_alloc(SubGhzEnvironment* environment);

void subghz_protocol_encoder_mitsubishi_v0_free(void* context);

SubGhzProtocolStatus
    subghz_protocol_encoder_mitsubishi_v0_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_encoder_mitsubishi_v0_stop(void* context);

LevelDuration subghz_protocol_encoder_mitsubishi_v0_yield(void* context);
