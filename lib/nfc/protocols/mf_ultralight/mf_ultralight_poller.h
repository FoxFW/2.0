#pragma once

#include "mf_ultralight.h"
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfUltralightPoller MfUltralightPoller;

typedef enum {
    MfUltralightPollerEventTypeRequestMode,
    MfUltralightPollerEventTypeAuthRequest,
    MfUltralightPollerEventTypeAuthSuccess,
    MfUltralightPollerEventTypeAuthFailed,
    MfUltralightPollerEventTypeReadSuccess,
    MfUltralightPollerEventTypeReadFailed,
    MfUltralightPollerEventTypeRequestWriteData,
    MfUltralightPollerEventTypeCardMismatch,
    MfUltralightPollerEventTypeCardLocked,
    MfUltralightPollerEventTypeWriteSuccess,
    MfUltralightPollerEventTypeWriteFail,
    MfUltralightPollerEventTypeRequestKey,
    MfUltralightPollerEventTypeWriteKeyRequest,
} MfUltralightPollerEventType;

typedef enum {
    MfUltralightPollerModeRead,
    MfUltralightPollerModeWrite,
    MfUltralightPollerModeDictAttack,
} MfUltralightPollerMode;

typedef struct {
    MfUltralightAuthPassword password;
    MfUltralightC3DesAuthKey tdes_key;
    MfUltralightAuthPack pack;
    bool auth_success;
    bool skip_auth;
} MfUltralightPollerAuthContext;

typedef struct {
    MfUltralightC3DesAuthKey key;
    bool key_provided;
} MfUltralightPollerKeyRequestData;

typedef union {
    MfUltralightPollerAuthContext auth_context;
    MfUltralightError error;
    const MfUltralightData* write_data;
    MfUltralightPollerMode poller_mode;
    MfUltralightPollerKeyRequestData key_request_data;
    bool write_key_skip;
} MfUltralightPollerEventData;

typedef struct {
    MfUltralightPollerEventType type;
    MfUltralightPollerEventData* data;
} MfUltralightPollerEvent;

MfUltralightError mf_ultralight_poller_auth_pwd(
    MfUltralightPoller* instance,
    MfUltralightPollerAuthContext* data);

MfUltralightError mf_ultralight_poller_authenticate_start(
    MfUltralightPoller* instance,
    const uint8_t* RndA,
    uint8_t* output);

MfUltralightError mf_ultralight_poller_authenticate_end(
    MfUltralightPoller* instance,
    const uint8_t* RndB,
    const uint8_t* request,
    uint8_t* response);

MfUltralightError mf_ultralight_poller_read_page(
    MfUltralightPoller* instance,
    uint8_t start_page,
    MfUltralightPageReadCommandData* data);

MfUltralightError mf_ultralight_poller_read_page_from_sector(
    MfUltralightPoller* instance,
    uint8_t sector,
    uint8_t tag,
    MfUltralightPageReadCommandData* data);

MfUltralightError mf_ultralight_poller_write_page(
    MfUltralightPoller* instance,
    uint8_t page,
    const MfUltralightPage* data);

MfUltralightError
    mf_ultralight_poller_read_version(MfUltralightPoller* instance, MfUltralightVersion* data);

MfUltralightError
    mf_ultralight_poller_read_signature(MfUltralightPoller* instance, MfUltralightSignature* data);

MfUltralightError mf_ultralight_poller_read_counter(
    MfUltralightPoller* instance,
    uint8_t counter_num,
    MfUltralightCounter* data);

MfUltralightError mf_ultralight_poller_read_tearing_flag(
    MfUltralightPoller* instance,
    uint8_t tearing_falg_num,
    MfUltralightTearingFlag* data);

#ifdef __cplusplus
}
#endif
