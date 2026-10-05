#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "slix_bundle/slix_device_defs.h"   /* nfc_device_slix */
#include "slix_bundle/slix_poller_defs.h"   /* nfc_poller_slix */
#include "slix_bundle/slix_listener_defs.h" /* nfc_listener_slix */

/* FoxFW NFC externalization - SLIX protocol group (.fal). Full stack bundled
 * byte-identical. iso15693_3_* (incl. _is_equal_uid, _poller_get_data) resolve
 * from firmware API (iso15693 base stays internal). */
static const NfcProtocol grp_slix_protocols[] = {NfcProtocolSlix};
static const NfcPollerBase* const grp_slix_pollers[] = {&nfc_poller_slix};
static const NfcListenerBase* const grp_slix_listeners[] = {&nfc_listener_slix};
static const NfcDeviceBase* const grp_slix_devices[] = {&nfc_device_slix};
static const NfcGroupPlugin grp_slix_plugin = {
    .count = COUNT_OF(grp_slix_protocols), .protocols = grp_slix_protocols,
    .pollers = grp_slix_pollers, .listeners = grp_slix_listeners, .devices = grp_slix_devices};
static const FlipperAppPluginDescriptor grp_slix_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_slix_plugin};
const FlipperAppPluginDescriptor* grp_nfc_slix_ep(void) { return &grp_slix_descriptor; }
