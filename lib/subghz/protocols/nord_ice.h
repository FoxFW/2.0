#pragma once

#include "base.h"

/*
 * Nord ICE fixed-code gate/garage remote. 300us short / 800us long timing,
 * 33-bit frame with a ~25*te_short inter-frame gap. Serial packs bits
 * [32:15] and [8:0] (26 bits total), button is bits [14:9] (6 bits).
 * Not rolling - identical value on every press of a given button.
 */
#define SUBGHZ_PROTOCOL_NORD_ICE_NAME "Nord ICE"

typedef struct SubGhzProtocolDecoderNordIce SubGhzProtocolDecoderNordIce;
typedef struct SubGhzProtocolEncoderNordIce SubGhzProtocolEncoderNordIce;

extern const SubGhzProtocolDecoder subghz_protocol_nord_ice_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_nord_ice_encoder;
extern const SubGhzProtocol subghz_protocol_nord_ice;

void* subghz_protocol_encoder_nord_ice_alloc(SubGhzEnvironment* environment);

void subghz_protocol_encoder_nord_ice_free(void* context);

SubGhzProtocolStatus
    subghz_protocol_encoder_nord_ice_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_encoder_nord_ice_stop(void* context);

LevelDuration subghz_protocol_encoder_nord_ice_yield(void* context);

void* subghz_protocol_decoder_nord_ice_alloc(SubGhzEnvironment* environment);

void subghz_protocol_decoder_nord_ice_free(void* context);

void subghz_protocol_decoder_nord_ice_reset(void* context);

void subghz_protocol_decoder_nord_ice_feed(void* context, bool level, uint32_t duration);

uint8_t subghz_protocol_decoder_nord_ice_get_hash_data(void* context);

SubGhzProtocolStatus subghz_protocol_decoder_nord_ice_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    subghz_protocol_decoder_nord_ice_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_decoder_nord_ice_get_string(void* context, FuriString* output);
