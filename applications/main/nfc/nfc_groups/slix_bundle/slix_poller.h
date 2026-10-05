#pragma once

#include "slix.h"

#include <nfc/nfc_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SlixPoller SlixPoller;

typedef enum {
    SlixPollerEventTypeError,
    SlixPollerEventTypePrivacyUnlockRequest,
    SlixPollerEventTypeReady,
} SlixPollerEventType;

typedef struct {
    SlixPassword password;
    bool password_set;
} SlixPollerEventDataPrivacyUnlockContext;

typedef union {
    SlixError error;
    SlixPollerEventDataPrivacyUnlockContext privacy_password;
} SlixPollerEventData;

typedef struct {
    SlixPollerEventType type;
    SlixPollerEventData* data;
} SlixPollerEvent;

SlixError slix_poller_send_frame(
    SlixPoller* instance,
    const BitBuffer* tx_data,
    BitBuffer* rx_data,
    uint32_t fwt);

SlixError slix_poller_get_nxp_system_info(SlixPoller* instance, SlixSystemInfo* data);

SlixError slix_poller_read_signature(SlixPoller* instance, SlixSignature* data);

SlixError slix_poller_get_random_number(SlixPoller* instance, SlixRandomNumber* data);

SlixError slix_poller_set_password(
    SlixPoller* instance,
    SlixPasswordType type,
    SlixPassword password,
    SlixRandomNumber random_number);

#ifdef __cplusplus
}
#endif
