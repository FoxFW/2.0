#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "emv_bundle/emv.h"
#include "emv_bundle/emv_poller_defs.h"

static const NfcProtocol grp_emv_protocols[] = {NfcProtocolEmv};
static const NfcPollerBase* const grp_emv_pollers[] = {&emv_poller};
static const NfcListenerBase* const grp_emv_listeners[] = {NULL};
static const NfcDeviceBase* const grp_emv_devices[] = {&nfc_device_emv};
static const NfcGroupPlugin grp_emv_plugin = {
    .count = COUNT_OF(grp_emv_protocols), .protocols = grp_emv_protocols,
    .pollers = grp_emv_pollers, .listeners = grp_emv_listeners, .devices = grp_emv_devices};
static const FlipperAppPluginDescriptor grp_emv_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_emv_plugin};
const FlipperAppPluginDescriptor* grp_nfc_emv_ep(void) { return &grp_emv_descriptor; }
