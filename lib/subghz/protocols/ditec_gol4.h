#pragma once

#include "base.h"

/*
 * Ditec GOL4 rolling-code gate remote. 400us short / 1100us long timing,
 * 54-bit frame, ~22*te_long start gap, start bit = 2*te_short HIGH. Rolling
 * code via an LCG-based scramble (not AES) over a 7-byte raw block packing
 * a 32-bit serial, 4-bit button, and 16-bit counter. Includes its own d-pad
 * custom-button remap (Up/Down/Left/Right/OK), ported as-is from ARF.
 */
#define SUBGHZ_PROTOCOL_DITEC_GOL4_NAME "Ditec GOL4"

typedef struct SubGhzProtocolDecoderDitecGOL4 SubGhzProtocolDecoderDitecGOL4;
typedef struct SubGhzProtocolEncoderDitecGOL4 SubGhzProtocolEncoderDitecGOL4;

extern const SubGhzProtocolDecoder subghz_protocol_ditec_gol4_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_ditec_gol4_encoder;
extern const SubGhzProtocol subghz_protocol_ditec_gol4;

void* subghz_protocol_encoder_ditec_gol4_alloc(SubGhzEnvironment* environment);

void subghz_protocol_encoder_ditec_gol4_free(void* context);

SubGhzProtocolStatus
    subghz_protocol_encoder_ditec_gol4_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_encoder_ditec_gol4_stop(void* context);

LevelDuration subghz_protocol_encoder_ditec_gol4_yield(void* context);

void* subghz_protocol_decoder_ditec_gol4_alloc(SubGhzEnvironment* environment);

void subghz_protocol_decoder_ditec_gol4_free(void* context);

void subghz_protocol_decoder_ditec_gol4_reset(void* context);

void subghz_protocol_decoder_ditec_gol4_feed(void* context, bool level, uint32_t duration);

uint8_t subghz_protocol_decoder_ditec_gol4_get_hash_data(void* context);

SubGhzProtocolStatus subghz_protocol_decoder_ditec_gol4_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    subghz_protocol_decoder_ditec_gol4_deserialize(void* context, FlipperFormat* flipper_format);

void subghz_protocol_decoder_ditec_gol4_get_string(void* context, FuriString* output);
