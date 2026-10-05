#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "st25tb_bundle/st25tb.h"              /* nfc_device_st25tb */
#include "st25tb_bundle/st25tb_poller_defs.h"  /* nfc_poller_st25tb */

/* FoxFW NFC externalization - ST25TB protocol group (.fal). Childless base, full
 * stack bundled byte-identical. No listener. iso14443_crc resolves from firmware API. */
static const NfcProtocol grp_st25tb_protocols[] = {NfcProtocolSt25tb};
static const NfcPollerBase* const grp_st25tb_pollers[] = {&nfc_poller_st25tb};
static const NfcListenerBase* const grp_st25tb_listeners[] = {NULL};
static const NfcDeviceBase* const grp_st25tb_devices[] = {&nfc_device_st25tb};
static const NfcGroupPlugin grp_st25tb_plugin = {
    .count = COUNT_OF(grp_st25tb_protocols), .protocols = grp_st25tb_protocols,
    .pollers = grp_st25tb_pollers, .listeners = grp_st25tb_listeners, .devices = grp_st25tb_devices};
static const FlipperAppPluginDescriptor grp_st25tb_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID, .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_st25tb_plugin};
const FlipperAppPluginDescriptor* grp_nfc_st25tb_ep(void) { return &grp_st25tb_descriptor; }
