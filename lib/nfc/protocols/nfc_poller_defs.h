#pragma once

#include "nfc_poller_base.h"
#include "nfc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const NfcPollerBase* const nfc_pollers_api[NfcProtocolNum];

const NfcPollerBase* nfc_poller_get_base(NfcProtocol protocol);

#ifdef __cplusplus
}
#endif
