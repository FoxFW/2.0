#pragma once

#include "nfc_listener_base.h"
#include "nfc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const NfcListenerBase* const nfc_listeners_api[NfcProtocolNum];

const NfcListenerBase* nfc_listener_get_base(NfcProtocol protocol);

#ifdef __cplusplus
}
#endif
