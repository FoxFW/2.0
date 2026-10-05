#pragma once

#include <gui/scene_manager.h>
#include <lib/nfc/protocols/nfc_protocol.h>

#include "nfc_protocol_support_common.h"

typedef struct NfcProtocolSupport NfcProtocolSupport;

/* FoxFW: pre-load (warm) the protocol-support GUI plugin for `protocol` now, so
 * it grabs a contiguous heap block before later activity (e.g. the supported-
 * cards cache sweep) fragments the heap. The plugin is then reused by the scenes.
 * Fixes the intermittent "Plugin Not Loaded" on saved-card load. */
void nfc_protocol_support_preload(NfcProtocol protocol, void* context);

void nfc_protocol_support_free(void* context);

void nfc_protocol_support_on_enter(NfcProtocolSupportScene scene, void* context);

bool nfc_protocol_support_on_event(
    NfcProtocolSupportScene scene,
    void* context,
    SceneManagerEvent event);

void nfc_protocol_support_on_exit(NfcProtocolSupportScene scene, void* context);

bool nfc_protocol_support_has_feature(
    NfcProtocol protocol,
    void* context,
    NfcProtocolFeature feature);

void nfc_protocol_support_extra_on_enter(NfcProtocol protocol, size_t index, void* context);

bool nfc_protocol_support_extra_on_event(
    NfcProtocol protocol,
    size_t index,
    void* context,
    SceneManagerEvent event);

void nfc_protocol_support_extra_on_exit(NfcProtocol protocol, size_t index, void* context);
