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

#define RENAULT_PROTOCOL_V1_NAME "Renault V1"

typedef struct SubGhzProtocolDecoderRenaultV1 SubGhzProtocolDecoderRenaultV1;
typedef struct SubGhzProtocolEncoderRenaultV1 SubGhzProtocolEncoderRenaultV1;

extern const SubGhzProtocol renault_v1_protocol;

void* subghz_protocol_decoder_renault_v1_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_renault_v1_reset(void* context);
void subghz_protocol_decoder_renault_v1_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_renault_v1_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_renault_v1_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus
    subghz_protocol_decoder_renault_v1_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_decoder_renault_v1_get_string(void* context, FuriString* output);

void* subghz_protocol_encoder_renault_v1_alloc(SubGhzEnvironment* environment);
void subghz_protocol_encoder_renault_v1_free(void* context);
SubGhzProtocolStatus
    subghz_protocol_encoder_renault_v1_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_encoder_renault_v1_stop(void* context);
LevelDuration subghz_protocol_encoder_renault_v1_yield(void* context);

#define RENAULT_V1_SEED_RECOVERED_NO      0U
#define RENAULT_V1_SEED_RECOVERED_YES     1U
#define RENAULT_V1_SEED_RECOVERED_BF_MISS 2U

typedef bool (*Hitag2SeedProgressCallback)(uint8_t progress, uint32_t cand_tested, void* context);

bool subghz_protocol_renault_v1_run_seed_bf(
    uint64_t data,
    uint64_t data_2,
    uint32_t serial,
    uint8_t btn,
    uint32_t cnt,
    uint32_t* seed_out);

bool subghz_protocol_renault_v1_run_seed_bf_ex(
    uint64_t data,
    uint64_t data_2,
    uint32_t serial,
    uint8_t btn,
    uint32_t cnt,
    uint32_t* seed_out,
    Hitag2SeedProgressCallback progress_cb,
    void* progress_ctx);
