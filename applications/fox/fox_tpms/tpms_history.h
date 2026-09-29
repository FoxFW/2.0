#pragma once

#include <math.h>
#include <furi.h>
#include <furi_hal.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/types.h>
#include "protocols/tpms_generic.h"

typedef struct TPMSHistory TPMSHistory;

typedef enum {
    TPMSHistoryStateAddKeyUnknown,
    TPMSHistoryStateAddKeyTimeOut,
    TPMSHistoryStateAddKeyNewDada,
    TPMSHistoryStateAddKeyUpdateData,
    TPMSHistoryStateAddKeyOverflow,
} TPMSHistoryStateAddKey;

TPMSHistory* tpms_history_alloc(void);

void tpms_history_free(TPMSHistory* instance);

void tpms_history_reset(TPMSHistory* instance);

uint32_t tpms_history_get_frequency(TPMSHistory* instance, uint16_t idx);

SubGhzRadioPreset* tpms_history_get_radio_preset(TPMSHistory* instance, uint16_t idx);

const char* tpms_history_get_preset(TPMSHistory* instance, uint16_t idx);

uint16_t tpms_history_get_item(TPMSHistory* instance);

uint8_t tpms_history_get_type_protocol(TPMSHistory* instance, uint16_t idx);

const char* tpms_history_get_protocol_name(TPMSHistory* instance, uint16_t idx);

void tpms_history_get_text_item_menu(TPMSHistory* instance, FuriString* output, uint16_t idx);

bool tpms_history_get_text_space_left(TPMSHistory* instance, FuriString* output);

TPMSHistoryStateAddKey
    tpms_history_add_to_history(TPMSHistory* instance, void* context, SubGhzRadioPreset* preset);

FlipperFormat* tpms_history_get_raw_data(TPMSHistory* instance, uint16_t idx);

bool tpms_history_replace_payload(TPMSHistory* instance, uint16_t idx, TPMSBlockGeneric* generic);
