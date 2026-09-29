#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "tpms_generic.h"
#include <lib/subghz/blocks/math.h>

#define TPMS_PROTOCOL_PMV107J_NAME "Toyota PMV-107J"

extern const SubGhzProtocolDecoder tpms_protocol_pmv107j_decoder;
extern const SubGhzProtocolEncoder tpms_protocol_pmv107j_encoder;
extern const SubGhzProtocol tpms_protocol_pmv107j;

void* tpms_protocol_decoder_pmv107j_alloc(SubGhzEnvironment* environment);
void tpms_protocol_decoder_pmv107j_free(void* context);
void tpms_protocol_decoder_pmv107j_reset(void* context);
void tpms_protocol_decoder_pmv107j_feed(void* context, bool level, uint32_t duration);
uint8_t tpms_protocol_decoder_pmv107j_get_hash_data(void* context);

SubGhzProtocolStatus tpms_protocol_decoder_pmv107j_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    tpms_protocol_decoder_pmv107j_deserialize(void* context, FlipperFormat* flipper_format);

void tpms_protocol_decoder_pmv107j_get_string(void* context, FuriString* output);

void tpms_protocol_pmv107j_pack(TPMSBlockGeneric* generic);
