#include "nfc_group_manager.h"
#include "nfc_group_plugin.h"

#include <furi/furi.h>
#include <storage/storage.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>

#define TAG "NfcGroupManager"

#define NFC_GROUP_PLUGINS_DIR EXT_PATH("apps_data/nfc/plugins")

/*
 * One slot per distinct .fal group. Each holds its OWN PluginManager so it can
 * be freed independently (the PluginManager API has no per-plugin unload). Add
 * a slot per group in Phase 2; add a proto->slot row below for every protocol
 * that group carries.
 */
typedef struct {
    const char* fal_name;
    PluginManager* pm;
    const NfcGroupPlugin* plugin;
    int refcount;
} NfcGroupSlot;

static NfcGroupSlot nfc_group_slots[] = {
    {"grp_nfc_felica.fal", NULL, NULL, 0},
    {"grp_nfc_emv.fal", NULL, NULL, 0},
    {"grp_nfc_mf_plus.fal", NULL, NULL, 0},
    {"grp_nfc_slix.fal", NULL, NULL, 0},
    {"grp_nfc_st25tb.fal", NULL, NULL, 0},
    {"grp_nfc_mf_desfire.fal", NULL, NULL, 0},
    {"grp_nfc_ntag4xx.fal", NULL, NULL, 0},
    {"grp_nfc_type_4_tag.fal", NULL, NULL, 0},
};

typedef struct {
    NfcProtocol protocol;
    size_t slot;
} NfcGroupProtoMap;

/* Protocols NOT listed here are internal (served by the static registries). */
static const NfcGroupProtoMap nfc_group_proto_map[] = {
    {NfcProtocolFelica, 0},
    {NfcProtocolEmv, 1},
    {NfcProtocolMfPlus, 2},
    {NfcProtocolSlix, 3},
    {NfcProtocolSt25tb, 4},
    {NfcProtocolMfDesfire, 5},
    {NfcProtocolNtag4xx, 6},
    {NfcProtocolType4Tag, 7},
};

static int nfc_group_slot_for(NfcProtocol protocol) {
    for(size_t i = 0; i < COUNT_OF(nfc_group_proto_map); i++) {
        if(nfc_group_proto_map[i].protocol == protocol) {
            return (int)nfc_group_proto_map[i].slot;
        }
    }
    return -1;
}

bool nfc_group_is_external(NfcProtocol protocol) {
    return nfc_group_slot_for(protocol) >= 0;
}

/* Free every resident group not currently in use (refcount 0), except keep_si.
 * Reclaims heap when a .fal load fails (OOM) so the requested group fits — peak
 * RAM stays ~one group, which is the whole point of on-demand group loading. */
static void nfc_group_evict_unused(int keep_si) {
    for(size_t i = 0; i < COUNT_OF(nfc_group_slots); i++) {
        if((int)i == keep_si) continue;
        NfcGroupSlot* s = &nfc_group_slots[i];
        if(s->pm != NULL && s->refcount == 0) {
            plugin_manager_free(s->pm);
            s->pm = NULL;
            s->plugin = NULL;
            FURI_LOG_D(TAG, "Evicted %s (reclaim heap)", s->fal_name);
        }
    }
}

static bool nfc_group_try_load(NfcGroupSlot* slot) {
    PluginManager* pm = plugin_manager_alloc(
        NFC_GROUP_PLUGIN_APP_ID, NFC_GROUP_PLUGIN_API_VERSION, firmware_api_interface);

    char path[128];
    snprintf(path, sizeof(path), "%s/%s", NFC_GROUP_PLUGINS_DIR, slot->fal_name);

    const PluginManagerError err = plugin_manager_load_single(pm, path);
    if(err != PluginManagerErrorNone) {
        plugin_manager_free(pm);
        return false;
    }

    const NfcGroupPlugin* plugin = plugin_manager_get_ep(pm, 0);
    if(plugin == NULL) {
        FURI_LOG_E(TAG, "No entry point in %s", slot->fal_name);
        plugin_manager_free(pm);
        return false;
    }

    slot->pm = pm;
    slot->plugin = plugin;
    FURI_LOG_I(TAG, "Loaded %s (%zu protocols)", slot->fal_name, plugin->count);
    return true;
}

bool nfc_group_ensure_loaded(NfcProtocol protocol) {
    const int si = nfc_group_slot_for(protocol);
    if(si < 0) return true; /* internal protocol — nothing to load */

    NfcGroupSlot* slot = &nfc_group_slots[si];

    if(slot->plugin == NULL) {
        /* ON-DEMAND INVARIANT: only ONE external group resident at a time. Free
         * every idle (refcount 0) group BEFORE loading this one, so the group's
         * ELF sections AND the poller allocation that follows have maximum heap.
         * This is what stops a card that sweeps several external groups (e.g. a
         * bare ISO14443-3A card probing every 4A child: MfPlus/MfDesfire/Ntag4xx/
         * Type4Tag/Emv) from accumulating groups and crashing on OOM. A group
         * that stays requested back-to-back is NOT reloaded (plugin != NULL
         * short-circuits above), so the active read of a detected card is cheap. */
        nfc_group_evict_unused(si);
        if(!nfc_group_try_load(slot)) {
            /* Backstop: a ref may have just dropped — evict again and retry once. */
            FURI_LOG_W(TAG, "Load %s failed; evicting idle groups, retrying", slot->fal_name);
            nfc_group_evict_unused(si);
            if(!nfc_group_try_load(slot)) {
                FURI_LOG_E(TAG, "Failed to load %s after evict", slot->fal_name);
                return false;
            }
        }
    }

    slot->refcount++;
    return true;
}

void nfc_group_release(NfcProtocol protocol) {
    const int si = nfc_group_slot_for(protocol);
    if(si < 0) return;

    NfcGroupSlot* slot = &nfc_group_slots[si];
    if(slot->refcount > 0) {
        slot->refcount--;
    }
    /* Keep-resident at refcount 0 (pilot): avoids reload churn on the scanner's
     * repeated base-protocol probes. Freed in nfc_group_manager_reset(); a
     * Phase 3 low-RAM watchdog may evict ref-0 groups early. */
}

static const NfcGroupPlugin* nfc_group_plugin_for(NfcProtocol protocol, size_t* idx_out) {
    const int si = nfc_group_slot_for(protocol);
    if(si < 0) return NULL;

    const NfcGroupPlugin* plugin = nfc_group_slots[si].plugin;
    if(plugin == NULL) return NULL;

    for(size_t i = 0; i < plugin->count; i++) {
        if(plugin->protocols[i] == protocol) {
            if(idx_out) *idx_out = i;
            return plugin;
        }
    }
    return NULL;
}

const NfcPollerBase* nfc_group_get_poller(NfcProtocol protocol) {
    size_t idx = 0;
    const NfcGroupPlugin* plugin = nfc_group_plugin_for(protocol, &idx);
    return plugin ? plugin->pollers[idx] : NULL;
}

const NfcListenerBase* nfc_group_get_listener(NfcProtocol protocol) {
    size_t idx = 0;
    const NfcGroupPlugin* plugin = nfc_group_plugin_for(protocol, &idx);
    return plugin ? plugin->listeners[idx] : NULL;
}

const NfcDeviceBase* nfc_group_get_device(NfcProtocol protocol) {
    size_t idx = 0;
    const NfcGroupPlugin* plugin = nfc_group_plugin_for(protocol, &idx);
    return plugin ? plugin->devices[idx] : NULL;
}

void nfc_group_manager_reset(void) {
    for(size_t i = 0; i < COUNT_OF(nfc_group_slots); i++) {
        if(nfc_group_slots[i].pm != NULL) {
            plugin_manager_free(nfc_group_slots[i].pm);
            nfc_group_slots[i].pm = NULL;
            nfc_group_slots[i].plugin = NULL;
            nfc_group_slots[i].refcount = 0;
            FURI_LOG_D(TAG, "Unloaded %s", nfc_group_slots[i].fal_name);
        }
    }
}
