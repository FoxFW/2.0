#pragma once

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include "tpms_generic.h"
#include <lib/subghz/blocks/math.h>

#define TPMS_PROTOCOL_RENAULT_NAME "Renault TPMS"

extern const SubGhzProtocolDecoder tpms_protocol_renault_decoder;
extern const SubGhzProtocolEncoder tpms_protocol_renault_encoder;
extern const SubGhzProtocol tpms_protocol_renault;

void* tpms_protocol_decoder_renault_alloc(SubGhzEnvironment* environment);
void tpms_protocol_decoder_renault_free(void* context);
void tpms_protocol_decoder_renault_reset(void* context);
void tpms_protocol_decoder_renault_feed(void* context, bool level, uint32_t duration);
uint8_t tpms_protocol_decoder_renault_get_hash_data(void* context);

SubGhzProtocolStatus tpms_protocol_decoder_renault_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    tpms_protocol_decoder_renault_deserialize(void* context, FlipperFormat* flipper_format);

void tpms_protocol_decoder_renault_get_string(void* context, FuriString* output);

void tpms_protocol_renault_pack(TPMSBlockGeneric* generic);
