#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "type_4_tag_bundle/type_4_tag.h"
#include "type_4_tag_bundle/type_4_tag_poller_defs.h"
#include "type_4_tag_bundle/type_4_tag_listener_defs.h"

static const NfcProtocol grp_type_4_tag_protocols[] = {NfcProtocolType4Tag};
static const NfcPollerBase* const grp_type_4_tag_pollers[] = {&type_4_tag_poller};
static const NfcListenerBase* const grp_type_4_tag_listeners[] = {&nfc_listener_type_4_tag};
static const NfcDeviceBase* const grp_type_4_tag_devices[] = {&nfc_device_type_4_tag};
static const NfcGroupPlugin grp_type_4_tag_plugin = {
    .count = COUNT_OF(grp_type_4_tag_protocols), .protocols = grp_type_4_tag_protocols,
    .pollers = grp_type_4_tag_pollers, .listeners = grp_type_4_tag_listeners, .devices = grp_type_4_tag_devices};
static const FlipperAppPluginDescriptor grp_type_4_tag_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_type_4_tag_plugin};
const FlipperAppPluginDescriptor* grp_nfc_type_4_tag_ep(void) { return &grp_type_4_tag_descriptor; }
