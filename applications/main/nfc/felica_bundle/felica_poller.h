#pragma once

#include "felica.h"
#include <lib/nfc/nfc.h>

#include <nfc/nfc_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FelicaPoller FelicaPoller;

typedef enum {
    FelicaPollerEventTypeError,
    FelicaPollerEventTypeReady,
    FelicaPollerEventTypeIncomplete,
    FelicaPollerEventTypeRequestAuthContext,
} FelicaPollerEventType;

typedef union {
    FelicaError error;
    FelicaAuthenticationContext* auth_context;
} FelicaPollerEventData;

typedef struct {
    FelicaPollerEventType type;
    FelicaPollerEventData* data;
} FelicaPollerEvent;

FelicaError felica_poller_activate(FelicaPoller* instance, FelicaData* data);

FelicaError felica_poller_read_blocks(
    FelicaPoller* instance,
    const uint8_t block_count,
    const uint8_t* const block_numbers,
    uint16_t service_code,
    FelicaPollerReadCommandResponse** const response_ptr);

#ifdef __cplusplus
}
#endif
