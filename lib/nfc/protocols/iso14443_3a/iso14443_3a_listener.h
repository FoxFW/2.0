#pragma once

#include "iso14443_3a.h"
#include <nfc/nfc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Iso14443_3aListener Iso14443_3aListener;

typedef enum {
    Iso14443_3aListenerEventTypeFieldOff,
    Iso14443_3aListenerEventTypeHalted,

    Iso14443_3aListenerEventTypeReceivedStandardFrame,
    Iso14443_3aListenerEventTypeReceivedData,
} Iso14443_3aListenerEventType;

typedef struct {
    BitBuffer* buffer;
} Iso14443_3aListenerEventData;

typedef struct {
    Iso14443_3aListenerEventType type;
    Iso14443_3aListenerEventData* data;
} Iso14443_3aListenerEvent;

Iso14443_3aError
    iso14443_3a_listener_tx(Iso14443_3aListener* instance, const BitBuffer* tx_buffer);

Iso14443_3aError iso14443_3a_listener_tx_with_custom_parity(
    Iso14443_3aListener* instance,
    const BitBuffer* tx_buffer);

Iso14443_3aError iso14443_3a_listener_send_standard_frame(
    Iso14443_3aListener* instance,
    const BitBuffer* tx_buffer);

#ifdef __cplusplus
}
#endif
