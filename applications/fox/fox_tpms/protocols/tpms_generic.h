#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <lib/flipper_format/flipper_format.h>
#include "furi.h"
#include <furi_hal.h>
#include <lib/subghz/types.h>
#include <locale/locale.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TPMS_NO_BATT 0xFF

typedef struct TPMSBlockGeneric TPMSBlockGeneric;

struct TPMSBlockGeneric {
    const char* protocol_name;
    uint64_t data;
    uint8_t data_count_bit;

    uint32_t timestamp;

    uint32_t id;
    uint8_t battery_low;

    float pressure;
    float temperature;
};

void tpms_block_generic_get_preset_name(const char* preset_name, FuriString* preset_str);

SubGhzProtocolStatus tpms_block_generic_serialize(
    TPMSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

SubGhzProtocolStatus
    tpms_block_generic_deserialize(TPMSBlockGeneric* instance, FlipperFormat* flipper_format);

SubGhzProtocolStatus tpms_block_generic_deserialize_check_count_bit(
    TPMSBlockGeneric* instance,
    FlipperFormat* flipper_format,
    uint16_t count_bit);

#ifdef __cplusplus
}
#endif
