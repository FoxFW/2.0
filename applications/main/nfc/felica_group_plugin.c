#include <furi.h>
#include <nfc/nfc_group_plugin.h>
#include <flipper_application/flipper_application.h>

#include "felica_bundle/felica.h"               /* nfc_device_felica */
#include "felica_bundle/felica_poller_defs.h"   /* nfc_poller_felica */
#include "felica_bundle/felica_listener_defs.h" /* nfc_listener_felica */

/*
 * FoxFW NFC externalization - Felica protocol group (.fal).
 *
 * Loaded on demand by the firmware NFC engine (lib/nfc/nfc_group_manager.c)
 * from apps_data/nfc/plugins/ to supply Felica's poller/listener/device
 * vtables, which were removed from the static firmware registries
 * (nfc_poller_defs.c / nfc_listener_defs.c / nfc_device_defs.c). The full
 * Felica stack is bundled here (felica_bundle/) byte-for-byte identical to the
 * firmware original, so "if it worked internally it works externally" holds by
 * construction. mbedtls_des3_* resolves from the firmware API.
 */

static const NfcProtocol grp_nfc_felica_protocols[] = {
    NfcProtocolFelica,
};

static const NfcPollerBase* const grp_nfc_felica_pollers[] = {
    &nfc_poller_felica,
};

static const NfcListenerBase* const grp_nfc_felica_listeners[] = {
    &nfc_listener_felica,
};

static const NfcDeviceBase* const grp_nfc_felica_devices[] = {
    &nfc_device_felica,
};

static const NfcGroupPlugin grp_nfc_felica_plugin = {
    .count = COUNT_OF(grp_nfc_felica_protocols),
    .protocols = grp_nfc_felica_protocols,
    .pollers = grp_nfc_felica_pollers,
    .listeners = grp_nfc_felica_listeners,
    .devices = grp_nfc_felica_devices,
};

static const FlipperAppPluginDescriptor grp_nfc_felica_descriptor = {
    .appid = NFC_GROUP_PLUGIN_APP_ID,
    .ep_api_version = NFC_GROUP_PLUGIN_API_VERSION,
    .entry_point = &grp_nfc_felica_plugin,
};

const FlipperAppPluginDescriptor* grp_nfc_felica_ep(void) {
    return &grp_nfc_felica_descriptor;
}
