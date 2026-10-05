#pragma once

/*
 * FoxFW NFC externalization — group plugin descriptor.
 *
 * A .fal protocol group exports one NfcGroupPlugin: for each NfcProtocol it
 * carries, the SAME poller/listener/device vtable pointers the static firmware
 * registries used to hold. The engine's group manager loads the .fal on demand
 * and merges these into the live registry views (nfc_*_get_base()).
 *
 * GOVERNING RULE — equivalence by construction: the protocol code compiled into
 * a group .fal is byte-for-byte identical to the former firmware code (only
 * #include paths are rewritten to SDK-absolute form, which cannot change
 * behaviour), and the vtables below are the identical structs the static
 * registry pointed to. Moving a protocol to a group therefore changes only
 * WHERE its code lives (flash -> SD, loaded into RAM on demand), never WHAT it
 * does. "If it worked internally, it works externally" holds by construction;
 * each insertion is verified by diffing the bundled sources against the firmware
 * originals (only includes differ) and tracing the call paths.
 */

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
    size_t count;                              /* number of protocols in this group */
    const NfcProtocol* protocols;              /* [count] which protocols */
    const NfcPollerBase* const* pollers;       /* [count] parallel to protocols */
    const NfcListenerBase* const* listeners;   /* [count] parallel to protocols */
    const NfcDeviceBase* const* devices;       /* [count] parallel to protocols */
} NfcGroupPlugin;

#ifdef __cplusplus
}
#endif
