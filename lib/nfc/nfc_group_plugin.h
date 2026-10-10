#pragma once


#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/nfc_poller_base.h>
#include <nfc/protocols/nfc_listener_base.h>
#include <nfc/protocols/nfc_device_base_i.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NFC_GROUP_PLUGIN_APP_ID      "nfc_group"
#define NFC_GROUP_PLUGIN_API_VERSION 1

typedef struct {
    size_t count;
    const NfcProtocol* protocols;
    const NfcPollerBase* const* pollers;
    const NfcListenerBase* const* listeners;
    const NfcDeviceBase* const* devices;
} NfcGroupPlugin;

#ifdef __cplusplus
}
#endif
