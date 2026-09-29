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

#define MITSUBISHI_PROTOCOL_V0A_NAME "Mitsubishi V0-a"

typedef struct SubGhzProtocolDecoderMitsubishiV0a SubGhzProtocolDecoderMitsubishiV0a;
typedef struct SubGhzProtocolEncoderMitsubishiV0a SubGhzProtocolEncoderMitsubishiV0a;

extern const SubGhzProtocol subghz_protocol_mitsubishi_v0a;

void* subghz_protocol_decoder_mitsubishi_v0a_alloc(SubGhzEnvironment* environment);

void subghz_protocol_decoder_mitsubishi_v0a_free(void* context);

void subghz_protocol_decoder_mitsubishi_v0a_reset(void* context);

void subghz_protocol_decoder_mitsubishi_v0a_feed(void* context, bool level, uint32_t duration);

uint8_t subghz_protocol_decoder_mitsubishi_v0a_get_hash_data(void* context);

SubGhzProtocolStatus subghz_protocol_decoder_mitsubishi_v0a_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    subghz_protocol_decoder_mitsubishi_v0a_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_decoder_mitsubishi_v0a_get_string(void* context, FuriString* output);

void* subghz_protocol_encoder_mitsubishi_v0a_alloc(SubGhzEnvironment* environment);

void subghz_protocol_encoder_mitsubishi_v0a_free(void* context);

SubGhzProtocolStatus
    subghz_protocol_encoder_mitsubishi_v0a_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_encoder_mitsubishi_v0a_stop(void* context);

LevelDuration subghz_protocol_encoder_mitsubishi_v0a_yield(void* context);
