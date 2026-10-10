#pragma once


#include "protocols/nfc_protocol.h"
#include "protocols/nfc_poller_base.h"
#include "protocols/nfc_listener_base.h"
#include "protocols/nfc_device_base_i.h"

#ifdef __cplusplus
extern "C" {
#endif

bool nfc_group_is_external(NfcProtocol protocol);

bool nfc_group_ensure_loaded(NfcProtocol protocol);

void nfc_group_release(NfcProtocol protocol);

const NfcPollerBase* nfc_group_get_poller(NfcProtocol protocol);
const NfcListenerBase* nfc_group_get_listener(NfcProtocol protocol);
const NfcDeviceBase* nfc_group_get_device(NfcProtocol protocol);

void nfc_group_manager_reset(void);

#ifdef __cplusplus
}
#endif
