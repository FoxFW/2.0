#pragma once

/*
 * FoxFW NFC externalization — on-demand protocol-group loader (engine side).
 *
 * Mirrors lib/subghz/devices/registry.c: a firmware library that uses the
 * PluginManager / ELF loader to pull a .fal plugin off the SD at runtime. Here
 * each .fal is an NFC protocol group (see nfc_group_plugin.h) carrying the
 * poller/listener/device vtables for one or more protocols.
 *
 * Lifecycle / RAM policy (pilot):
 *   - ensure_loaded(P): if P is INTERNAL -> no-op true. If P is EXTERNAL -> load
 *     its group's .fal (once) and ref++. Returns false only if the .fal could
 *     not be loaded (missing/corrupt/mismatch).
 *   - release(P): ref--. The group is kept RESIDENT at ref 0 (no load churn
 *     across the scanner's repeated base-protocol probes). Actual unload happens
 *     in nfc_group_manager_reset() (NFC engine teardown) — Phase 3 adds a
 *     low-RAM watchdog that may evict ref-0 groups early.
 *   - get_poller/listener/device(P): the loaded group's vtable for P, or NULL if
 *     P's group is not currently loaded. Called by the nfc_*_get_base()
 *     accessors as the external branch.
 *
 * Each group gets its OWN PluginManager so it can be freed independently (the
 * PluginManager API has no per-plugin unload, only free-all).
 */

#include "protocols/nfc_protocol.h"
#include "protocols/nfc_poller_base.h"
#include "protocols/nfc_listener_base.h"
#include "protocols/nfc_device_base_i.h"

#ifdef __cplusplus
extern "C" {
#endif

/* True if `protocol` is served by an external .fal group (not the static
 * firmware registries). Authoritative source for the internal/external split —
 * used instead of NULL-testing the static tables, because a static listener
 * entry can legitimately be NULL for an internal protocol. */
bool nfc_group_is_external(NfcProtocol protocol);

/* Ensure the group providing `protocol` is loaded; ref++. No-op (true) for
 * internal protocols. Returns false if the group's .fal failed to load. */
bool nfc_group_ensure_loaded(NfcProtocol protocol);

/* Release one ensure_loaded(); ref--. No-op for internal protocols. */
void nfc_group_release(NfcProtocol protocol);

/* Vtable views for an external protocol; NULL if its group is not loaded. */
const NfcPollerBase* nfc_group_get_poller(NfcProtocol protocol);
const NfcListenerBase* nfc_group_get_listener(NfcProtocol protocol);
const NfcDeviceBase* nfc_group_get_device(NfcProtocol protocol);

/* Free every loaded group (unload all .fal, release all RAM). Called on NFC
 * engine teardown. Safe to call when nothing is loaded. */
void nfc_group_manager_reset(void);

#ifdef __cplusplus
}
#endif
