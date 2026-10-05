#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "mf_desfire_bundle/mf_desfire.h"              /* nfc_device_mf_desfire */
#include "mf_desfire_bundle/mf_desfire_poller_defs.h"  /* mf_desfire_poller */

/* FoxFW NFC externalization - Mifare DESFire protocol group (.fal). Full stack
 * bundled byte-identical (incl. nxp_native_command). No listener.
 * iso14443_4a_* resolve from firmware API. */
static const NfcProtocol grp_mf_desfire_protocols[] = {NfcProtocolMfDesfire};
static const NfcPollerBase* const grp_mf_desfire_pollers[] = {&mf_desfire_poller};
static const NfcListenerBase* const grp_mf_desfire_listeners[] = {NULL};
static const NfcDeviceBase* const grp_mf_desfire_devices[] = {&nfc_device_mf_desfire};
static const NfcGroupPlugin grp_mf_desfire_plugin = {
    .count = COUNT_OF(grp_mf_desfire_protocols), .protocols = grp_mf_desfire_protocols,
    .pollers = grp_mf_desfire_pollers, .listeners = grp_mf_desfire_listeners, .devices = grp_mf_desfire_devices};
static const FlipperAppPluginDescriptor grp_mf_desfire_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_mf_desfire_plugin};
const FlipperAppPluginDescriptor* grp_nfc_mf_desfire_ep(void) { return &grp_mf_desfire_descriptor; }
