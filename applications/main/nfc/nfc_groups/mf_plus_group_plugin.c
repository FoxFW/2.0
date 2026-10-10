#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "mf_plus_bundle/mf_plus.h"
#include "mf_plus_bundle/mf_plus_poller_defs.h"
#include "mf_plus_bundle/mf_plus_listener_defs.h"

static const NfcProtocol grp_mf_plus_protocols[] = {NfcProtocolMfPlus};
static const NfcPollerBase* const grp_mf_plus_pollers[] = {&mf_plus_poller};
static const NfcListenerBase* const grp_mf_plus_listeners[] = {&mf_plus_listener};
static const NfcDeviceBase* const grp_mf_plus_devices[] = {&nfc_device_mf_plus};
static const NfcGroupPlugin grp_mf_plus_plugin = {
    .count = COUNT_OF(grp_mf_plus_protocols), .protocols = grp_mf_plus_protocols,
    .pollers = grp_mf_plus_pollers, .listeners = grp_mf_plus_listeners, .devices = grp_mf_plus_devices};
static const FlipperAppPluginDescriptor grp_mf_plus_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_mf_plus_plugin};
const FlipperAppPluginDescriptor* grp_nfc_mf_plus_ep(void) { return &grp_mf_plus_descriptor; }
