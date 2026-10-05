#pragma once

#include "mf_plus.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfPlusListener MfPlusListener;

typedef enum {
    MfPlusListenerEventTypeUpdate,
} MfPlusListenerEventType;

typedef union {
    void* reserved;
} MfPlusListenerEventData;

typedef struct {
    MfPlusListenerEventType type;
    MfPlusListenerEventData* data;
} MfPlusListenerEvent;

#ifdef __cplusplus
}
#endif
