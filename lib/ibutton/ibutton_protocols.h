#pragma once

#include <stdint.h>
#include <stddef.h>

#include "protocols/protocol_common.h"

#include "ibutton_key.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct iButtonProtocols iButtonProtocols;

iButtonProtocols* ibutton_protocols_alloc(void);

void ibutton_protocols_free(iButtonProtocols* protocols);

uint32_t ibutton_protocols_get_protocol_count(void);

size_t ibutton_protocols_get_max_data_size(iButtonProtocols* protocols);

iButtonProtocolId ibutton_protocols_get_id_by_name(iButtonProtocols* protocols, const char* name);

const char* ibutton_protocols_get_manufacturer(iButtonProtocols* protocols, iButtonProtocolId id);

const char* ibutton_protocols_get_name(iButtonProtocols* protocols, iButtonProtocolId id);

uint32_t ibutton_protocols_get_features(iButtonProtocols* protocols, iButtonProtocolId id);

bool ibutton_protocols_read(iButtonProtocols* protocols, iButtonKey* key);

bool ibutton_protocols_write_id(iButtonProtocols* protocols, iButtonKey* key);

bool ibutton_protocols_write_copy(iButtonProtocols* protocols, iButtonKey* key);

void ibutton_protocols_emulate_start(iButtonProtocols* protocols, iButtonKey* key);

void ibutton_protocols_emulate_stop(iButtonProtocols* protocols, iButtonKey* key);

bool ibutton_protocols_save(
    iButtonProtocols* protocols,
    const iButtonKey* key,
    const char* file_name);

bool ibutton_protocols_load(iButtonProtocols* protocols, iButtonKey* key, const char* file_name);

void ibutton_protocols_render_uid(
    iButtonProtocols* protocols,
    const iButtonKey* key,
    FuriString* result);

void ibutton_protocols_render_data(
    iButtonProtocols* protocols,
    const iButtonKey* key,
    FuriString* result);

void ibutton_protocols_render_brief_data(
    iButtonProtocols* protocols,
    const iButtonKey* key,
    FuriString* result);

void ibutton_protocols_render_error(
    iButtonProtocols* protocols,
    const iButtonKey* key,
    FuriString* result);

bool ibutton_protocols_is_valid(iButtonProtocols* protocols, const iButtonKey* key);

void ibutton_protocols_get_editable_data(
    iButtonProtocols* protocols,
    const iButtonKey* key,
    iButtonEditableData* editable);

void ibutton_protocols_apply_edits(iButtonProtocols* protocols, const iButtonKey* key);

#ifdef __cplusplus
}
#endif
