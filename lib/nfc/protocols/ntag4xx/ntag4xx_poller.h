#pragma once

#include "ntag4xx.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ntag4xxPoller Ntag4xxPoller;

typedef enum {
    Ntag4xxPollerEventTypeReadSuccess,
    Ntag4xxPollerEventTypeReadFailed,
} Ntag4xxPollerEventType;

typedef union {
    Ntag4xxError error;
} Ntag4xxPollerEventData;

typedef struct {
    Ntag4xxPollerEventType type;
    Ntag4xxPollerEventData* data;
} Ntag4xxPollerEvent;

#ifdef __cplusplus
}
#endif
