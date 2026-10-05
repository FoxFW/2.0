#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "ntag4xx_bundle/ntag4xx.h"              /* nfc_device_ntag4xx */
#include "ntag4xx_bundle/ntag4xx_poller_defs.h"  /* ntag4xx_poller */

/* FoxFW NFC externalization - NTAG4xx protocol group (.fal). Full stack bundled
 * byte-identical (incl. nxp_native_command). No listener. iso14443_4a_* from API. */
static const NfcProtocol grp_ntag4xx_protocols[] = {NfcProtocolNtag4xx};
static const NfcPollerBase* const grp_ntag4xx_pollers[] = {&ntag4xx_poller};
static const NfcListenerBase* const grp_ntag4xx_listeners[] = {NULL};
static const NfcDeviceBase* const grp_ntag4xx_devices[] = {&nfc_device_ntag4xx};
static const NfcGroupPlugin grp_ntag4xx_plugin = {
    .count = COUNT_OF(grp_ntag4xx_protocols), .protocols = grp_ntag4xx_protocols,
    .pollers = grp_ntag4xx_pollers, .listeners = grp_ntag4xx_listeners, .devices = grp_ntag4xx_devices};
static const FlipperAppPluginDescriptor grp_ntag4xx_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_ntag4xx_plugin};
const FlipperAppPluginDescriptor* grp_nfc_ntag4xx_ep(void) { return &grp_ntag4xx_descriptor; }
