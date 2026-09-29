#pragma once

#include <core/string.h>

#include "protocols/protocol_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct iButtonKey iButtonKey;

iButtonKey* ibutton_key_alloc(size_t data_size);

void ibutton_key_free(iButtonKey* key);

iButtonProtocolId ibutton_key_get_protocol_id(const iButtonKey* key);

void ibutton_key_set_protocol_id(iButtonKey* key, iButtonProtocolId protocol_id);

void ibutton_key_reset(iButtonKey* key);

#ifdef __cplusplus
}
#endif
