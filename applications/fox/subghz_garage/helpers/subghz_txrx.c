#include "subghz_txrx_i.h" // IWYU pragma: keep
#include "subghz_debug_log.h"

#include <math.h>
#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include "subghz_devices_lazy_compat.h"
#include "subghz_custom_btn_compat.h"
#include "subghz_lib_ext_compat.h"
#include "subghz_memmgr_pool_compat.h"
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>

#define TAG "SubGhzTxRx"

/* Cached by SubGhz_Garage_cc1101_check.c, launched periodically in the
 * background by Desktop and on every Mode Picker -> Garage launch - see
 * subghz_txrx_ensure_radio_init() below. */
#define CC1101_EXT_STATUS_PATH EXT_PATH("subghz/.cc1101_ext_status")

static void subghz_txrx_radio_device_power_on(SubGhzTxRx* instance);
static void subghz_txrx_radio_device_power_off(SubGhzTxRx* instance);
static void subghz_txrx_ensure_external_device(SubGhzTxRx* instance);

/* Minimum free heap required before rebuilding the receiver against a real
 * (non-empty) protocol group - each group is capped at 4 protocols plus
 * Raw/BinRaw (always present), each decoder a small struct, so the worst
 * case is only a couple KB. The rest of the margin is for qFlipper, which
 * (per user testing) can eat 4-8KB of free heap while connected - USB CLI
 * and lab.flipper.net's web CLI don't have this problem, only qFlipper
 * itself. See subghz_txrx_ensure_protocol_group().
 *
 * Same flat 14000 as SUBGHZ_LOW_RAM_FREE_HEAP (subghz_i.h) - this only
 * actually evaluates on a fresh group load or group switch, not every RX
 * cycle, and every caller (Read/Read RAW switching to the RAW decoder,
 * loading a saved file, redisplaying a captured/saved signal) reaches it
 * with the RX worker either not yet running or already stopped, so there's
 * no separate lower tier needed here the way subghz_low_ram_mitigate()
 * needed one for the CLI-lock-list's own RX-worker-driven floor. */
#define SUBGHZ_TXRX_RECEIVER_REBUILD_FREE_HEAP 14000

/* How long to wait before the one retry attempt in
 * subghz_txrx_ensure_protocol_group() - gives transient RAM users (like
 * qFlipper's own polling) a chance to release memory back before trying
 * again. */
#define SUBGHZ_TXRX_PROTOCOL_GROUP_RETRY_DELAY_MS 2000

static const SubGhzProtocolRegistry subghz_garage_empty_protocol_registry = {
    .items = NULL,
    .size = 0,
};

/* Radio device (internal CC1101), environment, and receiver used to be
 * allocated unconditionally in subghz_txrx_alloc() - ProtoPirate_FoxEdition
 * defers all three the same way (see protopirate_radio_init(), gated on
 * app->radio_initialized), so a session that never leaves the Start menu,
 * or only browses Saved/Protocols/Settings, never pays for them. Called at
 * the top of every subghz_txrx.c entry point that touches
 * instance->radio_device/environment/receiver, directly or indirectly -
 * cheap no-op after the first call. */
static void subghz_txrx_ensure_radio_init(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->radio_initialized) {
        return;
    }
    instance->radio_initialized = true;

    FURI_LOG_I(TAG, "Radio init: free heap %zu", memmgr_get_free_heap());
    subghz_debug_log_write("Radio init: free heap %zu", memmgr_get_free_heap());

    /* Same reasoning as subghz_txrx_ensure_external_device() for the
     * external plugin scan: probing/initializing the radio subsystem at
     * all has a real cost and was previously paid on every single launch,
     * even one that never uses the radio.
     *
     * subghz_devices_init_internal_only() hard-crashes via furi_check() if
     * the shared device registry (a global singleton - lib/subghz/devices/
     * devices.c) is already initialized - see subghz_devices_deinit()'s own
     * bracket in subghz_txrx_free() below, and SubGhz_Garage_cc1101_check.c
     * for the other code that touches this same registry. Logged tightly
     * around the call itself (not just the wider "Radio init"/"Radio init
     * done" pair around it) so a crash here is unambiguous: the debug log
     * would show "About to init device registry" with no matching "Device
     * registry init done" line after it. */
    subghz_debug_log_write("About to init device registry");
    subghz_garage_devices_init_radio_only();
    subghz_debug_log_write("Device registry init done");
    instance->radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    instance->radio_device_type = SubGhzRadioDeviceTypeInternal;

    /* If SubGhz_Garage_cc1101_check last found an external module attached,
     * trust that cached answer and load the external plugin - loading it
     * via subghz_txrx_ensure_external_device() (idempotent, no SPI probe)
     * instead of subghz_txrx_radio_device_is_external_connected(), which
     * would re-run subghz_devices_load_external()'s ~25KB .fal scan the
     * probe app already paid for, defeating the point of caching it. A
     * "0"/missing/unreadable flag just leaves the internal device active,
     * as set above - zero extra cost for users with nothing plugged in.
     *
     * BUT: the flag can go stale between the probe writing it and this
     * code running - the classic case being the module physically
     * unplugged in between. Unlike the plugin load above, is_connect()
     * itself is just one cheap SPI register read (microseconds, not a
     * ~25KB reload - see subghz_device_cc1101_ext_is_connect()), so there's
     * no real cost argument against calling it here too, and skipping it
     * is not safe: subghz_device_cc1101_ext_idle() (called from
     * subghz_txrx_begin() at the start of every Read/RX/TX) does
     * furi_check(cc1101_wait_status_state(...)) - a hard crash, not a
     * graceful failure, if the chip never answers because it isn't there.
     * So verify for real before committing to external, and correct the
     * stale flag on the way past so the status icon stops lying too. */
    Storage* ext_flag_storage = furi_record_open(RECORD_STORAGE);
    File* ext_flag_file = storage_file_alloc(ext_flag_storage);
    if(storage_file_open(ext_flag_file, CC1101_EXT_STATUS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char flag = 0;
        storage_file_read(ext_flag_file, &flag, 1);
        storage_file_close(ext_flag_file);
        if(flag == '1') {
            subghz_txrx_ensure_external_device(instance);
            subghz_txrx_radio_device_power_on(instance);
            const SubGhzDevice* ext_device =
                subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
            bool still_connected = ext_device && subghz_devices_is_connect(ext_device);
            if(still_connected) {
                instance->radio_device = ext_device;
                subghz_devices_begin(instance->radio_device);
                instance->radio_device_type = SubGhzRadioDeviceTypeExternalCC1101;
            } else {
                // Cached flag said yes, live check says no - stay on the
                // internal device already set above, and fix the cached
                // flag so the status icon and the next launch both agree.
                FURI_LOG_W(
                    TAG, "Cached CC1101 external flag was stale - falling back to internal");
                subghz_txrx_radio_device_power_off(instance);
                if(storage_file_open(
                       ext_flag_file, CC1101_EXT_STATUS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    char c = '0';
                    storage_file_write(ext_flag_file, &c, 1);
                    storage_file_close(ext_flag_file);
                }
            }
        }
    }
    storage_file_free(ext_flag_file);
    furi_record_close(RECORD_STORAGE);

    instance->environment = subghz_environment_alloc();
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        instance->environment, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        instance->environment, SUBGHZ_NICE_FLOR_S_DIR_NAME);
    subghz_environment_set_protocol_registry(
        instance->environment, (void*)instance->protocol_registry);
    instance->receiver = subghz_receiver_alloc_init(instance->environment);

    /* Re-apply whatever filter/rx callback subghz_alloc() cached before the
     * receiver existed (subghz_txrx_receiver_set_filter() /
     * subghz_txrx_set_rx_callback() are called unconditionally at app
     * boot) - same pattern already used when the receiver gets rebuilt in
     * subghz_txrx_try_load_protocol_group(). */
    if(instance->receiver_filter_set) {
        subghz_receiver_set_filter(instance->receiver, instance->receiver_filter);
    }
    if(instance->rx_callback) {
        subghz_receiver_set_rx_callback(
            instance->receiver, instance->rx_callback, instance->rx_callback_context);
    }

    FURI_LOG_I(TAG, "Radio init done: free heap %zu", memmgr_get_free_heap());
    subghz_debug_log_write("Radio init done: free heap %zu", memmgr_get_free_heap());
}

static void subghz_txrx_unload_protocol_plugin(SubGhzTxRx* instance) {
    furi_assert(instance);

    instance->protocol_plugin = NULL;
    instance->protocol_registry = &subghz_garage_empty_protocol_registry;

    bool had_plugin = instance->protocol_plugin_manager != NULL;

    if(instance->protocol_plugin_manager) {
        plugin_manager_free(instance->protocol_plugin_manager);
        instance->protocol_plugin_manager = NULL;
    }
    if(instance->protocol_plugin_resolver) {
        composite_api_resolver_free(instance->protocol_plugin_resolver);
        instance->protocol_plugin_resolver = NULL;
    }

    /* Only log if there was actually something to unload - this runs
     * unconditionally from subghz_txrx_free() even when Read was never
     * entered this session, which would otherwise flood every single app
     * exit with a no-op pool reading. */
    if(had_plugin) {
        FURI_LOG_I(
            TAG,
            "Protocol group plugin unloaded: pool free %zu, pool max block %zu",
            subghz_garage_pool_get_free(),
            subghz_garage_pool_get_max_block());
        subghz_debug_log_write(
            "Protocol group plugin unloaded: pool free %zu, pool max block %zu",
            subghz_garage_pool_get_free(),
            subghz_garage_pool_get_max_block());
    }
}

/* NOT called from subghz_txrx_ensure_protocol_group() - only 4 protocols
 * (FAAC SLH, Beninca ARC, Jarolift, KingGates Stylo 4K) ever touch the
 * keystore, and only for enriching a captured signal's info-screen text
 * (get_string) or an actual TX ("Send" always pays this cost - see
 * subghz_txrx_tx_start()). Plain bit-level RX decode never reads it for
 * any protocol - a signal is captured and added to history with or
 * without it. So the ~27KB parse stays lazy, not paid just because RX
 * started. */
void subghz_txrx_ensure_keystore(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);

    if(instance->keystore_loaded) {
        return;
    }
    instance->keystore_loaded = true;

    FURI_LOG_I(TAG, "Loading keystore: free heap %zu", memmgr_get_free_heap());
    subghz_environment_load_keystore(instance->environment, SUBGHZ_KEYSTORE_DIR_NAME);
    subghz_environment_load_keystore(instance->environment, SUBGHZ_KEYSTORE_DIR_USER_NAME);
    FURI_LOG_I(TAG, "Keystore loaded: free heap %zu", memmgr_get_free_heap());
}

/* Shared by subghz_txrx_ensure_protocol_group() and
 * subghz_txrx_ensure_tx_protocol_plugin() - both just alloc a resolver +
 * PluginManager, load one .fal, and fetch its entry point; only what they
 * do with the result differs. Kept in the always-resident main .fap, so
 * this is the one copy rather than two near-identical ones. */
static const SubGhzGarageProtocolPlugin* subghz_txrx_load_garage_plugin(
    const char* path,
    PluginManager** out_manager,
    CompositeApiResolver** out_resolver) {
    CompositeApiResolver* resolver = composite_api_resolver_alloc();
    if(!resolver) {
        FURI_LOG_E(TAG, "Failed to allocate plugin resolver");
        return NULL;
    }
    composite_api_resolver_add(resolver, firmware_api_interface);

    PluginManager* manager = plugin_manager_alloc(
        SUBGHZ_GARAGE_PROTOCOL_PLUGIN_APP_ID,
        SUBGHZ_GARAGE_PROTOCOL_PLUGIN_API_VERSION,
        composite_api_resolver_get(resolver));
    if(!manager) {
        FURI_LOG_E(TAG, "Failed to allocate plugin manager");
        composite_api_resolver_free(resolver);
        return NULL;
    }

    FURI_LOG_I(
        TAG,
        "Loading %s: free heap %zu, max free block %zu, pool free %zu, pool max block %zu",
        path,
        memmgr_get_free_heap(),
        memmgr_heap_get_max_free_block(),
        subghz_garage_pool_get_free(),
        subghz_garage_pool_get_max_block());
    subghz_debug_log_write(
        "Loading %s: free heap %zu, max free block %zu, pool free %zu, pool max block %zu",
        path,
        memmgr_get_free_heap(),
        memmgr_heap_get_max_free_block(),
        subghz_garage_pool_get_free(),
        subghz_garage_pool_get_max_block());

    PluginManagerError error = plugin_manager_load_single(manager, path);
    if(error != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Failed to load plugin: %d, free heap %zu, pool free %zu", (int)error,
            memmgr_get_free_heap(), subghz_garage_pool_get_free());
        subghz_debug_log_write(
            "Failed to load plugin %s: error %d, free heap %zu, pool free %zu",
            path,
            (int)error,
            memmgr_get_free_heap(),
            subghz_garage_pool_get_free());
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return NULL;
    }

    FURI_LOG_I(
        TAG,
        "%s loaded: pool free %zu, pool max block %zu",
        path,
        subghz_garage_pool_get_free(),
        subghz_garage_pool_get_max_block());
    subghz_debug_log_write(
        "%s loaded: pool free %zu, pool max block %zu",
        path,
        subghz_garage_pool_get_free(),
        subghz_garage_pool_get_max_block());

    const SubGhzGarageProtocolPlugin* plugin = plugin_manager_get_ep(manager, 0U);
    if(!plugin) {
        FURI_LOG_E(TAG, "Plugin entry point is invalid");
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return NULL;
    }

    *out_manager = manager;
    *out_resolver = resolver;
    return plugin;
}

/* One attempt at loading `group`'s plugin and rebuilding the receiver
 * against it. Returns NULL - having freed anything it partially allocated,
 * and leaving instance untouched - if either step fails; the caller decides
 * whether to retry. Does not touch instance->protocol_plugin_load_failed or
 * fall back to the empty registry - that's the caller's job once it's
 * given up retrying. */
static const SubGhzGarageProtocolPlugin* subghz_txrx_try_load_protocol_group(
    SubGhzTxRx* instance,
    SubGhzGarageProtocolGroup group) {
    PluginManager* manager = NULL;
    CompositeApiResolver* resolver = NULL;
    const SubGhzGarageProtocolPlugin* plugin = subghz_txrx_load_garage_plugin(
        subghz_garage_protocol_group_paths[group], &manager, &resolver);
    if(!plugin || !plugin->registry) {
        return NULL;
    }
    FURI_LOG_I(TAG, "Plugin .fal loaded: free heap %zu", memmgr_get_free_heap());

    /* subghz_receiver_alloc_init() below mallocs a decoder struct per
     * protocol in the group (up to 4, plus Raw/BinRaw in every group) with
     * no OOM handling of its own - core malloc() furi_checks and crashes
     * outright if any of those fail. A few KB is normally nothing, but with
     * qFlipper connected eating into free heap on an already-tight device,
     * it's not guaranteed. Refuse instead of risking the crash. */
    if(memmgr_get_free_heap() < SUBGHZ_TXRX_RECEIVER_REBUILD_FREE_HEAP) {
        FURI_LOG_E(
            TAG,
            "Not enough free heap to rebuild receiver (%zu < %d)",
            memmgr_get_free_heap(),
            SUBGHZ_TXRX_RECEIVER_REBUILD_FREE_HEAP);
        plugin_manager_free(manager);
        composite_api_resolver_free(resolver);
        return NULL;
    }

    instance->protocol_plugin_resolver = resolver;
    instance->protocol_plugin_manager = manager;
    instance->protocol_plugin = plugin;
    instance->protocol_registry = plugin->registry;
    subghz_environment_set_protocol_registry(
        instance->environment, (void*)instance->protocol_registry);

    /* Old receiver (if any) was already freed by the caller, before the old
     * plugin was unloaded - rebuild it now that the environment points at
     * this group's protocols, and re-point the worker at the new receiver. */
    instance->receiver = subghz_receiver_alloc_init(instance->environment);
    FURI_LOG_I(TAG, "Receiver rebuilt against new group: free heap %zu", memmgr_get_free_heap());
    /* Worker may not exist yet (e.g. just browsing the Protocols list, RX
     * never started) - subghz_txrx_ensure_worker() wires it to whichever
     * receiver is current at the time RX actually starts, so there's
     * nothing to re-point here in that case. */
    if(instance->worker) {
        subghz_garage_worker_set_pair_callback(
            instance->worker, (SubGhzGarageWorkerPairCallback)subghz_receiver_decode);
        subghz_garage_worker_set_context(instance->worker, instance->receiver);
    }

    /* Re-apply whatever filter/rx callback were configured on the old
     * receiver before it got swapped out. */
    if(instance->receiver_filter_set) {
        subghz_receiver_set_filter(instance->receiver, instance->receiver_filter);
    }
    if(instance->rx_callback) {
        subghz_receiver_set_rx_callback(
            instance->receiver, instance->rx_callback, instance->rx_callback_context);
    }

    return plugin;
}

const SubGhzGarageProtocolPlugin*
    subghz_txrx_ensure_protocol_group(SubGhzTxRx* instance, SubGhzGarageProtocolGroup group) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);

    if(instance->protocol_plugin && instance->active_protocol_group == group) {
        return instance->protocol_plugin;
    }

    if(instance->protocol_plugin_load_failed && instance->active_protocol_group == group) {
        /* Already tried this group this session (load attempt, wait,
         * retry - see below) and both attempts failed - don't hammer an
         * already-fragmented heap with another near-certain-to-fail retry
         * on every re-entry. subghz_txrx_set_protocol_group() clears this
         * the moment the user actually picks a different group (or
         * reopens the app), which is the only thing that changes the
         * odds. */
        return NULL;
    }

    /* Free the old receiver (if any) before unloading the old plugin - its
     * decoder slots hold decoder->free function pointers that live inside
     * that plugin's loaded code (see subghz_receiver_alloc_init() in
     * lib/subghz/receiver.c), so freeing it after the unload would call
     * into memory that's already gone. */
    if(instance->receiver) {
        subghz_receiver_free(instance->receiver);
        instance->receiver = NULL;
    }
    if(instance->protocol_plugin) {
        subghz_txrx_unload_protocol_plugin(instance);
    }
    instance->active_protocol_group = group;

    const SubGhzGarageProtocolPlugin* plugin =
        subghz_txrx_try_load_protocol_group(instance, group);
    if(!plugin) {
        FURI_LOG_W(
            TAG,
            "Group %d load failed, free heap %zu - waiting %dms to retry once",
            (int)group,
            memmgr_get_free_heap(),
            SUBGHZ_TXRX_PROTOCOL_GROUP_RETRY_DELAY_MS);
        furi_delay_ms(SUBGHZ_TXRX_PROTOCOL_GROUP_RETRY_DELAY_MS);
        FURI_LOG_I(TAG, "Retrying group %d load: free heap %zu", (int)group, memmgr_get_free_heap());
        plugin = subghz_txrx_try_load_protocol_group(instance, group);
    }

    if(!plugin) {
        FURI_LOG_E(TAG, "Group %d load failed again after retry - giving up", (int)group);
        instance->protocol_plugin_load_failed = true;
        /* Rebuild against the empty registry so instance->receiver is
         * never left NULL, matching the invariant everywhere else in this
         * file - the caller is responsible for telling the user this
         * group has no protocols loaded (see subghz_txrx_rx_start()). */
        instance->protocol_registry = &subghz_garage_empty_protocol_registry;
        subghz_environment_set_protocol_registry(
            instance->environment, (void*)instance->protocol_registry);
        instance->receiver = subghz_receiver_alloc_init(instance->environment);
        return NULL;
    }

    instance->protocol_plugin_load_failed = false;
    return plugin;
}

const SubGhzGarageProtocolPlugin* subghz_txrx_ensure_protocol_plugin(SubGhzTxRx* instance) {
    furi_assert(instance);
    return subghz_txrx_ensure_protocol_group(instance, instance->active_protocol_group);
}

static void subghz_txrx_unload_tx_protocol_plugin(SubGhzTxRx* instance) {
    furi_assert(instance);

    instance->tx_protocol_plugin = NULL;

    if(instance->tx_protocol_plugin_manager) {
        plugin_manager_free(instance->tx_protocol_plugin_manager);
        instance->tx_protocol_plugin_manager = NULL;
    }
    if(instance->tx_protocol_plugin_resolver) {
        composite_api_resolver_free(instance->tx_protocol_plugin_resolver);
        instance->tx_protocol_plugin_resolver = NULL;
    }
}

const SubGhzGarageProtocolPlugin* subghz_txrx_ensure_tx_protocol_plugin(
    SubGhzTxRx* instance,
    SubGhzGarageTxProtocol tx_protocol) {
    furi_assert(instance);

    if(instance->tx_protocol_plugin_loaded && instance->active_tx_protocol == tx_protocol) {
        if(instance->tx_protocol_plugin) {
            subghz_environment_set_protocol_registry(
                instance->environment, (void*)instance->tx_protocol_plugin->registry);
        }
        return instance->tx_protocol_plugin;
    }

    if(instance->tx_protocol_plugin_loaded) {
        subghz_txrx_unload_tx_protocol_plugin(instance);
    }
    instance->tx_protocol_plugin_loaded = true;
    instance->active_tx_protocol = tx_protocol;

    PluginManager* manager = NULL;
    CompositeApiResolver* resolver = NULL;
    const SubGhzGarageProtocolPlugin* plugin = subghz_txrx_load_garage_plugin(
        subghz_garage_tx_protocol_paths[tx_protocol], &manager, &resolver);
    if(!plugin) {
        return NULL;
    }

    instance->tx_protocol_plugin_resolver = resolver;
    instance->tx_protocol_plugin_manager = manager;
    instance->tx_protocol_plugin = plugin;

    subghz_environment_set_protocol_registry(instance->environment, (void*)plugin->registry);

    return plugin;
}

/* subghz_txrx_ensure_tx_protocol_plugin() points the environment at a
 * single-protocol registry so subghz_transmitter_alloc_init() can find a
 * real encoder for it. Callers must call this again right after they're
 * done (encoder allocated, or gen_ function returned) so RX afterward
 * doesn't keep decoding against that 1-protocol registry instead of the
 * active RX group's. */
void subghz_txrx_restore_rx_protocol_registry(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_environment_set_protocol_registry(
        instance->environment, (void*)instance->protocol_registry);
}

void subghz_txrx_set_protocol_group(SubGhzTxRx* instance, SubGhzGarageProtocolGroup group) {
    furi_assert(instance);

    if(instance->active_protocol_group == group && instance->protocol_plugin) {
        return;
    }

    /* Lazy: just unload whatever's loaded (if anything) and remember the
     * new selection. The next actual need (RX start, TX, protocol list,
     * ...) loads it via subghz_txrx_ensure_protocol_plugin().
     *
     * instance->receiver's decoder slots (if any) hold decoder->free
     * function pointers that live inside the plugin being unloaded below
     * (see subghz_receiver_alloc_init() in lib/subghz/receiver.c) - it
     * must be freed first, same reasoning as subghz_txrx_free() and
     * subghz_txrx_ensure_protocol_group(). Rebuilt against the empty
     * registry (not left NULL) so nothing that touches instance->receiver
     * before the next real group load dereferences NULL. This was the
     * actual crash-on-group-switch bug - this function, not
     * ensure_protocol_group()'s own (already-fixed) unload/free ordering,
     * is what runs when the user picks a different group from the
     * Protocols or Receiver Config screen. */
    if(instance->protocol_plugin) {
        subghz_receiver_free(instance->receiver);
        subghz_txrx_unload_protocol_plugin(instance);
        /* unload_protocol_plugin() points instance->protocol_registry back
         * at the empty registry but doesn't touch the environment's own
         * registry pointer - without this, alloc_init below would read the
         * just-freed plugin's registry instead. */
        subghz_environment_set_protocol_registry(
            instance->environment, (void*)instance->protocol_registry);
        instance->receiver = subghz_receiver_alloc_init(instance->environment);
    }
    if(instance->active_protocol_group != group) {
        instance->protocol_plugin_load_failed = false;
    }
    instance->active_protocol_group = group;
}

SubGhzGarageProtocolGroup subghz_txrx_get_protocol_group(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->active_protocol_group;
}

void subghz_txrx_reset_protocol_load_failed(SubGhzTxRx* instance) {
    furi_assert(instance);
    instance->protocol_plugin_load_failed = false;
}

const SubGhzProtocolRegistry* subghz_txrx_get_protocol_registry(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_ensure_protocol_plugin(instance);
    return instance->protocol_registry;
}

static void subghz_txrx_radio_device_power_on(SubGhzTxRx* instance) {
    UNUSED(instance);
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        //CC1101 power-up time
        furi_delay_ms(10);
    }
}

static void subghz_txrx_radio_device_power_off(SubGhzTxRx* instance) {
    UNUSED(instance);
    if(furi_hal_power_is_otg_enabled()) furi_hal_power_disable_otg();
}

SubGhzTxRx* subghz_txrx_alloc(void) {
    FURI_LOG_I(TAG, "txrx_alloc start: free heap %zu", memmgr_get_free_heap());

    SubGhzTxRx* instance = malloc(sizeof(SubGhzTxRx));
    instance->protocol_plugin_load_failed = false;
    instance->setting = subghz_setting_alloc();
    /* Garage's own preset file (deduped - see setting_garage_full's own
     * comments) - NOT core Automotive's setting_user, which Garage doesn't
     * need and can't spare the heap for.
     *
     * Loaded ONCE, here, and never again for the rest of the app's
     * lifetime - subghz_setting_load() does a full reset-and-reload every
     * call, which frees every preset array entry (subghz_setting_preset_
     * reset() in lib/subghz/subghz_setting.c) and reallocates fresh ones.
     * subghz_txrx_set_preset() stores a raw pointer into that array
     * (preset->data = preset_data, no copy - see below), and several
     * places capture that pointer well before it's used (a saved file's
     * embedded preset in subghz_i.c, a Modulation dropdown pick in Radio
     * Settings). A second subghz_setting_load() call anywhere in the
     * session would silently turn any of those into a dangling pointer -
     * this was tried once already (a boot-time minimal file + an on-demand
     * reload to the full file) and is believed to be the cause of a
     * furi_check crash on exit after enough navigation to have picked up a
     * stale preset pointer. Not worth the ~1KB it would have deferred. */
    subghz_setting_load(instance->setting, EXT_PATH("subghz/assets/setting_garage_full"));

    FURI_LOG_I(TAG, "txrx_alloc after setting_load: free heap %zu", memmgr_get_free_heap());

    instance->preset = malloc(sizeof(SubGhzRadioPreset));
    instance->preset->name = furi_string_alloc();
    subghz_txrx_set_default_preset(instance, 0);

    instance->txrx_state = SubGhzTxRxStateSleep;

    subghz_txrx_hopper_set_state(instance, SubGhzHopperStateOFF);
    subghz_txrx_speaker_set_state(instance, SubGhzSpeakerStateDisable);
    subghz_txrx_set_debug_pin_state(instance, false);

    /* The worker (RX thread + its 8KB level-duration stream buffer, ~10.8KB
     * total) is the single biggest boot-time cost in this whole function -
     * bigger than the entire keystore. It only exists to consume the CC1101
     * RX callback stream while a capture is actually running, so it's
     * allocated lazily by subghz_txrx_ensure_worker() right before RX
     * starts, and freed again the moment RX stops - see subghz_txrx_rx()
     * and subghz_txrx_rx_end() below. This frees ~10.8KB of heap for the
     * entire time the user is anywhere other than an active RX/Read RAW
     * capture (browsing Saved, Protocols, Settings, etc.), which is exactly
     * when a ~10-19KB protocol group .fal needs room to load. */
    instance->worker = NULL;
    instance->fff_data = flipper_format_string_alloc();

    FURI_LOG_I(TAG, "txrx_alloc after fff_data: free heap %zu", memmgr_get_free_heap());

    /* The ~27KB KeeLoq manufacturer-code table itself is loaded lazily on
     * first actual RX/TX/protocol-list use (see subghz_txrx_ensure_keystore
     * in subghz_txrx_ensure_protocol_group) rather than here - only check
     * that the file exists, cheaply, so the app's launch-time "No SD card
     * or database found" gate (subghz_txrx_is_database_loaded) still works
     * without paying the full parse cost on every boot. */
    {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        instance->is_database_loaded = storage_file_exists(storage, SUBGHZ_KEYSTORE_DIR_NAME);
        furi_record_close(RECORD_STORAGE);
    }
    instance->keystore_loaded = false;

    /* Start with an empty registry - the real one lives in whichever
     * group's protocol plugin is currently selected (defaults to group 1)
     * and is loaded lazily on first use (see
     * subghz_txrx_ensure_protocol_plugin), so app launch and simple menu
     * browsing don't have to map any protocol group into RAM. */
    instance->active_protocol_group = SubGhzGarageProtocolGroup1;
    instance->protocol_plugin = NULL;
    instance->protocol_plugin_manager = NULL;
    instance->protocol_plugin_resolver = NULL;
    instance->protocol_registry = &subghz_garage_empty_protocol_registry;
    instance->tx_protocol_plugin_loaded = false;
    instance->tx_protocol_plugin = NULL;
    instance->tx_protocol_plugin_manager = NULL;
    instance->tx_protocol_plugin_resolver = NULL;
    instance->receiver_filter_set = false;
    instance->rx_callback = NULL;
    instance->rx_callback_context = NULL;

    /* Worker callbacks are wired in subghz_txrx_ensure_worker() instead of
     * here, since the worker itself doesn't exist yet at this point. */

    /* Radio device (internal CC1101), environment, and receiver are ALSO
     * deferred - not allocated until subghz_txrx_ensure_radio_init() runs,
     * on first actual need (Read, Read RAW, TX, protocol list, Radio
     * Settings' module toggle/frequency check, ...). Detecting/
     * initializing the external CC1101 module specifically stays deferred
     * even further, same as before - probing it means enabling OTG power
     * and running a live SPI transaction, unnecessary for the common case
     * (no external module attached) and previously intermittently crashing
     * or freezing app launch on this build - see
     * subghz_txrx_ensure_external_device(). */
    instance->radio_device = NULL;
    instance->radio_device_type = SubGhzRadioDeviceTypeInternal;
    instance->environment = NULL;
    instance->receiver = NULL;
    instance->radio_initialized = false;
    instance->external_device_loaded = false;

    FURI_LOG_I(TAG, "txrx_alloc end: free heap %zu", memmgr_get_free_heap());

    return instance;
}

void subghz_txrx_free(SubGhzTxRx* instance) {
    furi_assert(instance);

    /* If the radio was never touched this session (app opened and closed
     * without ever leaving the Start menu, or only Saved/Protocols/Settings
     * were browsed), radio_device/environment/receiver are all still NULL -
     * see subghz_txrx_ensure_radio_init(). Nothing below this point needs
     * freeing or deinitializing in that case. */
    if(instance->radio_initialized) {
        if(instance->radio_device_type != SubGhzRadioDeviceTypeInternal) {
            subghz_txrx_radio_device_power_off(instance);
            subghz_devices_end(instance->radio_device);
        }

        /* Bracketed the same way as its matching init call in
         * subghz_txrx_ensure_radio_init() - see that comment for why. */
        subghz_debug_log_write("About to deinit device registry");
        subghz_devices_deinit();
        subghz_debug_log_write("Device registry deinit done");

        if(instance->worker) {
            subghz_garage_worker_free(instance->worker);
        }
        /* instance->receiver's decoder slots hold decoder->alloc/free/feed
         * function pointers that live inside whichever group .fal is
         * currently loaded (see subghz_receiver_alloc_init() in
         * lib/subghz/receiver.c) - it must be freed (which calls
         * decoder->free() on each slot) before that plugin's code gets
         * unloaded below, or those calls land in freed memory. Doesn't
         * matter if the receiver was ever rebuilt against a real group
         * (empty-registry receiver has zero slots either way). */
        subghz_receiver_free(instance->receiver);

        subghz_environment_free(instance->environment);
    }

    subghz_txrx_unload_protocol_plugin(instance);
    if(instance->tx_protocol_plugin_loaded) {
        subghz_txrx_unload_tx_protocol_plugin(instance);
    }

    flipper_format_free(instance->fff_data);
    furi_string_free(instance->preset->name);
    subghz_setting_free(instance->setting);

    free(instance->preset);
    free(instance);
}

bool subghz_txrx_is_database_loaded(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->is_database_loaded;
}

void subghz_txrx_set_preset(
    SubGhzTxRx* instance,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size) {
    furi_assert(instance);
    furi_string_set(instance->preset->name, preset_name);

    SubGhzRadioPreset* preset = instance->preset;
    preset->frequency = frequency;
    preset->data = preset_data;
    preset->data_size = preset_data_size;
}

uint8_t*
    subghz_txrx_set_tx_power(uint8_t* preset_data, size_t preset_data_size, uint8_t tx_power) {
#define PRESET_POWER_OFFSET_FM 8
#define PRESET_POWER_OFFSET_AM 7
#define TX_PATABLE_OFFSET_AM   8
#define TX_PATABLE_COUNT       17

    //I had to skip the +10dBM and -6dBm Values, use only ones AM/FM have in common.
    //Highest Value is 12dBm for AM, 10 for FM. So Menu needs to reflect that.
    const uint8_t tx_pa_table[TX_PATABLE_COUNT] = {
        0,
        0xC0, //12dBm
        0xCD, //7dBm
        0x86, //5dBm
        0x50, //0dBm
        0x26, // -10dBm
        0x1D, // -15dBm
        0x17, //-20dBm
        0x03, //-30dBm
        0xC0, // 10dBm
        0xC8, //7dBm
        0x84, //5dBm
        0x60, //0dBm
        0x34, //-10dBm
        0x1D, //-15dBm
        0x0E, // -20dBm
        0x12, //-30dBm
    };

    //Grab the AM and FM byte now, so we can do proper checks.
    uint8_t fm_byte = preset_data[preset_data_size - PRESET_POWER_OFFSET_FM];
    uint8_t am_byte = preset_data[preset_data_size - PRESET_POWER_OFFSET_AM];

    //Set the TX Power Here in the CC1101 register...

    //If we have both bytes 1st bytes set or none, this isnt a preset we can deal with here.
    if(fm_byte && !am_byte) {
        //Use FM Table
        if(tx_power) {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_FM] =
                tx_pa_table[TX_PATABLE_OFFSET_AM + tx_power];
        } else {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_FM] =
                tx_pa_table[1]; //Max Power 0xC0 10dBm
        }
    } else if(am_byte && !fm_byte) {
        //Use AM Table
        if(tx_power) {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_AM] = tx_pa_table[tx_power];
        } else {
            preset_data[preset_data_size - PRESET_POWER_OFFSET_AM] =
                tx_pa_table[1]; //Max Power 0xC0 12dBm
        }
    }

    //Pass back the preset_so we can call one liners.
    return preset_data;
}

const char* subghz_txrx_get_preset_name(SubGhzTxRx* instance, const char* preset) {
    UNUSED(instance);
    const char* preset_name = "";
    if(!strcmp(preset, "FuriHalSubGhzPresetOok270Async")) {
        preset_name = "AM270";
    } else if(!strcmp(preset, "FuriHalSubGhzPresetOok650Async")) {
        preset_name = "AM650";
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev238Async")) {
        preset_name = "FM238";
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev12KAsync")) {
        preset_name = "FM12K";
    } else if(!strcmp(preset, "FuriHalSubGhzPreset2FSKDev476Async")) {
        preset_name = "FM476";
    } else if(!strcmp(preset, "FuriHalSubGhzPresetCustom")) {
        preset_name = "CUSTOM";
    } else {
        FURI_LOG_E(TAG, "Unknown preset");
    }
    return preset_name;
}

SubGhzRadioPreset subghz_txrx_get_preset(SubGhzTxRx* instance) {
    furi_assert(instance);
    return *instance->preset;
}

void subghz_txrx_get_frequency_and_modulation(
    SubGhzTxRx* instance,
    FuriString* frequency,
    FuriString* modulation,
    bool long_name) {
    furi_assert(instance);
    SubGhzRadioPreset* preset = instance->preset;
    if(frequency != NULL) {
        furi_string_printf(
            frequency,
            "%03ld.%02ld",
            preset->frequency / 1000000 % 1000,
            preset->frequency / 10000 % 100);
    }
    if(modulation != NULL) {
        if(long_name) {
            furi_string_printf(modulation, "%s", furi_string_get_cstr(preset->name));
        } else {
            furi_string_printf(modulation, "%.2s", furi_string_get_cstr(preset->name));
        }
    }
}

static void subghz_txrx_begin(SubGhzTxRx* instance, uint8_t* preset_data) {
    furi_assert(instance);
    FURI_LOG_I(TAG, "txrx_begin: radio_device=%p preset_data=%p",
        (void*)instance->radio_device, (void*)preset_data);
    subghz_debug_log_write(
        "txrx_begin: radio_device=%p preset_data=%p",
        (void*)instance->radio_device,
        (void*)preset_data);
    subghz_devices_reset(instance->radio_device);
    FURI_LOG_I(TAG, "txrx_begin: reset done");
    subghz_debug_log_write("txrx_begin: reset done");
    subghz_devices_idle(instance->radio_device);
    FURI_LOG_I(TAG, "txrx_begin: idle done, loading preset");
    subghz_debug_log_write("txrx_begin: idle done, loading preset");
    subghz_devices_load_preset(instance->radio_device, FuriHalSubGhzPresetCustom, preset_data);
    FURI_LOG_I(TAG, "txrx_begin: preset loaded");
    subghz_debug_log_write("txrx_begin: preset loaded");
    instance->txrx_state = SubGhzTxRxStateIDLE;
}

/* Allocates the worker (RX thread + 8KB stream buffer, ~10.8KB total) on
 * first use and wires it to the current receiver - see the comment above
 * `instance->worker = NULL` in subghz_txrx_alloc() for why this is lazy.
 * Freed again in subghz_txrx_rx_end() once RX stops. */
static void subghz_txrx_ensure_worker(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->worker) {
        FURI_LOG_I(TAG, "ensure_worker: already allocated (%p), skipping", (void*)instance->worker);
        return;
    }
    FURI_LOG_I(TAG, "Allocating worker: free heap %zu, receiver=%p",
        memmgr_get_free_heap(), (void*)instance->receiver);
    subghz_debug_log_write(
        "Allocating worker: free heap %zu, receiver=%p",
        memmgr_get_free_heap(),
        (void*)instance->receiver);
    instance->worker = subghz_garage_worker_alloc();
    FURI_LOG_I(TAG, "ensure_worker: alloc'd %p, wiring callbacks", (void*)instance->worker);
    subghz_debug_log_write("ensure_worker: alloc'd %p, wiring callbacks", (void*)instance->worker);
    subghz_garage_worker_set_overrun_callback(
        instance->worker, (SubGhzGarageWorkerOverrunCallback)subghz_receiver_reset);
    subghz_garage_worker_set_pair_callback(
        instance->worker, (SubGhzGarageWorkerPairCallback)subghz_receiver_decode);
    subghz_garage_worker_set_context(instance->worker, instance->receiver);
    FURI_LOG_I(TAG, "ensure_worker: done");
    subghz_debug_log_write("ensure_worker: done");
}

static uint32_t subghz_txrx_rx(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    furi_assert(
        instance->txrx_state != SubGhzTxRxStateRx && instance->txrx_state != SubGhzTxRxStateSleep);
    FURI_LOG_I(TAG, "txrx_rx: freq=%lu, ensuring worker", (unsigned long)frequency);
    subghz_debug_log_write("txrx_rx: freq=%lu, ensuring worker", (unsigned long)frequency);

    subghz_txrx_ensure_worker(instance);
    FURI_LOG_I(TAG, "txrx_rx: worker=%p, setting frequency", (void*)instance->worker);
    subghz_debug_log_write("txrx_rx: worker=%p, setting frequency", (void*)instance->worker);

    subghz_devices_idle(instance->radio_device);

    uint32_t value = subghz_devices_set_frequency(instance->radio_device, frequency);
    FURI_LOG_I(TAG, "txrx_rx: frequency set (%lu), flushing rx", (unsigned long)value);
    subghz_debug_log_write("txrx_rx: frequency set (%lu), flushing rx", (unsigned long)value);
    subghz_devices_flush_rx(instance->radio_device);
    subghz_txrx_speaker_on(instance);
    FURI_LOG_I(TAG, "txrx_rx: starting async rx");
    subghz_debug_log_write("txrx_rx: starting async rx");

    subghz_devices_start_async_rx(
        instance->radio_device, subghz_garage_worker_rx_callback, instance->worker);
    FURI_LOG_I(TAG, "txrx_rx: async rx started, starting worker");
    subghz_debug_log_write("txrx_rx: async rx started, starting worker");
    subghz_garage_worker_start(instance->worker);
    instance->txrx_state = SubGhzTxRxStateRx;
    FURI_LOG_I(TAG, "txrx_rx: worker started, done");
    subghz_debug_log_write("txrx_rx: worker started, done");
    return value;
}

static void subghz_txrx_idle(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->txrx_state != SubGhzTxRxStateSleep) {
        subghz_devices_idle(instance->radio_device);
        subghz_txrx_speaker_off(instance);
        instance->txrx_state = SubGhzTxRxStateIDLE;
    }
}

/* release_worker: false for the hopper's internal end/restart cycles (keep
 * the worker resident across a frequency/preset hop - freeing and
 * reallocating its thread + 8KB buffer on every hop would add needless
 * churn and heap fragmentation risk for something that's about to be
 * reallocated microseconds later). true for a genuine stop (leaving the
 * scene, user pressed Back, etc.) - see subghz_txrx_stop(). */
static void subghz_txrx_rx_end(SubGhzTxRx* instance, bool release_worker) {
    furi_assert(instance);
    furi_assert(instance->txrx_state == SubGhzTxRxStateRx);
    FURI_LOG_I(TAG, "rx_end: release_worker=%d worker=%p radio_device=%p",
        (int)release_worker, (void*)instance->worker, (void*)instance->radio_device);

    if(instance->worker && subghz_garage_worker_is_running(instance->worker)) {
        FURI_LOG_I(TAG, "rx_end: stopping worker");
        subghz_garage_worker_stop(instance->worker);
        FURI_LOG_I(TAG, "rx_end: worker stopped, stopping async rx");
        subghz_devices_stop_async_rx(instance->radio_device);
        FURI_LOG_I(TAG, "rx_end: async rx stopped");
    }
    if(release_worker && instance->worker) {
        /* Give the ~10.8KB back once RX genuinely stops - see
         * subghz_txrx_ensure_worker(). */
        FURI_LOG_I(TAG, "rx_end: freeing worker");
        subghz_garage_worker_free(instance->worker);
        instance->worker = NULL;
        FURI_LOG_I(TAG, "rx_end: worker freed");
    }
    subghz_devices_idle(instance->radio_device);
    subghz_txrx_speaker_off(instance);
    instance->txrx_state = SubGhzTxRxStateIDLE;
    FURI_LOG_I(TAG, "rx_end: done");
}

void subghz_txrx_sleep(SubGhzTxRx* instance) {
    furi_assert(instance);
    /* Nothing to put to sleep if the radio was never touched this session -
     * don't force-init it just to sleep it (called unconditionally from
     * subghz_free() on every app exit). */
    if(instance->radio_initialized) {
        subghz_devices_sleep(instance->radio_device);
    }
    instance->txrx_state = SubGhzTxRxStateSleep;
}

static bool subghz_txrx_tx(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    furi_assert(instance->txrx_state != SubGhzTxRxStateSleep);

    subghz_devices_idle(instance->radio_device);
    subghz_devices_set_frequency(instance->radio_device, frequency);

    bool ret = subghz_devices_set_tx(instance->radio_device);
    if(ret) {
        subghz_txrx_speaker_on(instance);
        instance->txrx_state = SubGhzTxRxStateTx;
    }

    return ret;
}

SubGhzTxRxStartTxState subghz_txrx_tx_start(SubGhzTxRx* instance, FlipperFormat* flipper_format) {
    furi_assert(instance);
    furi_assert(flipper_format);

    subghz_txrx_ensure_protocol_plugin(instance);
    /* TX (whether from "Add Manually" or replaying a saved file) is a
     * deliberate, rare action, not something that happens at boot - fine
     * to unconditionally pay the keystore cost here rather than track
     * which of the 4 protocols that need it this transmission is for. */
    subghz_txrx_ensure_keystore(instance);
    subghz_txrx_stop(instance);

    SubGhzTxRxStartTxState ret = SubGhzTxRxStartTxStateErrorParserOthers;
    FuriString* temp_str = furi_string_alloc();
    do {
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            break;
        }
        ret = SubGhzTxRxStartTxStateOk;

        SubGhzRadioPreset* preset = instance->preset;

        /* Alutech AT-4N, Somfy Telis, and Jarolift have a real dedicated
         * encoder split into their own TX plugin (see protocol_groups.h) -
         * point the environment at that single-protocol registry instead
         * of the active RX group's (whose copy of these 3 has a NULL-stub
         * encoder in RX-only builds) so the lookup below finds a working
         * one. */
        SubGhzGarageTxProtocol tx_protocol;
        bool using_tx_plugin_registry =
            subghz_garage_tx_protocol_for_name(furi_string_get_cstr(temp_str), &tx_protocol) &&
            subghz_txrx_ensure_tx_protocol_plugin(instance, tx_protocol) != NULL;

        instance->transmitter =
            subghz_transmitter_alloc_init(instance->environment, furi_string_get_cstr(temp_str));

        if(using_tx_plugin_registry) {
            subghz_txrx_restore_rx_protocol_registry(instance);
        }

        if(instance->transmitter) {
            if(subghz_transmitter_deserialize(instance->transmitter, flipper_format) ==
               SubGhzProtocolStatusOk) {
                if(strcmp(furi_string_get_cstr(preset->name), "") != 0) {
                    subghz_txrx_begin(
                        instance,
                        subghz_setting_get_preset_data_by_name(
                            instance->setting, furi_string_get_cstr(preset->name)));
                    if(preset->frequency) {
                        if(!subghz_txrx_tx(instance, preset->frequency)) {
                            FURI_LOG_E(TAG, "Only Rx");
                            ret = SubGhzTxRxStartTxStateErrorOnlyRx;
                        }
                    } else {
                        ret = SubGhzTxRxStartTxStateErrorParserOthers;
                    }

                } else {
                    FURI_LOG_E(
                        TAG, "Unknown name preset \" %s \"", furi_string_get_cstr(preset->name));
                    ret = SubGhzTxRxStartTxStateErrorParserOthers;
                }

                if(ret == SubGhzTxRxStartTxStateOk) {
                    //Start TX
                    subghz_devices_start_async_tx(
                        instance->radio_device, subghz_transmitter_yield, instance->transmitter);
                }
            } else {
                ret = SubGhzTxRxStartTxStateErrorParserOthers;
            }
        } else {
            ret = SubGhzTxRxStartTxStateErrorParserOthers;
        }
        if(ret != SubGhzTxRxStartTxStateOk) {
            subghz_transmitter_free(instance->transmitter);
            if(instance->txrx_state != SubGhzTxRxStateIDLE) {
                subghz_txrx_idle(instance);
            }
        }

    } while(false);
    furi_string_free(temp_str);
    return ret;
}

bool subghz_txrx_rx_start(SubGhzTxRx* instance) {
    furi_assert(instance);
    FURI_LOG_I(TAG, "rx_start: enter, preset name=\"%s\" freq=%lu",
        instance->preset && instance->preset->name ? furi_string_get_cstr(instance->preset->name) : "(null)",
        (unsigned long)(instance->preset ? instance->preset->frequency : 0));
    subghz_debug_log_write(
        "rx_start: enter, preset name=\"%s\" freq=%lu",
        instance->preset && instance->preset->name ? furi_string_get_cstr(instance->preset->name) : "(null)",
        (unsigned long)(instance->preset ? instance->preset->frequency : 0));
    bool protocol_ready = subghz_txrx_ensure_protocol_plugin(instance) != NULL;
    FURI_LOG_I(TAG, "rx_start: protocol_ready=%d, calling stop()", (int)protocol_ready);
    subghz_debug_log_write("rx_start: protocol_ready=%d, calling stop()", (int)protocol_ready);
    subghz_txrx_stop(instance);
    if(protocol_ready) {
        uint8_t* preset_data = subghz_setting_get_preset_data_by_name(
            subghz_txrx_get_setting(instance), furi_string_get_cstr(instance->preset->name));
        FURI_LOG_I(TAG, "rx_start: preset_data=%p, calling begin()", (void*)preset_data);
        subghz_debug_log_write("rx_start: preset_data=%p, calling begin()", (void*)preset_data);
        subghz_txrx_begin(instance, preset_data);
        FURI_LOG_I(TAG, "rx_start: begin() done, calling rx() at freq=%lu",
            (unsigned long)instance->preset->frequency);
        subghz_debug_log_write(
            "rx_start: begin() done, calling rx() at freq=%lu",
            (unsigned long)instance->preset->frequency);
        subghz_txrx_rx(instance, instance->preset->frequency);
        FURI_LOG_I(TAG, "rx_start: rx() done");
        subghz_debug_log_write("rx_start: rx() done");
    }
    FURI_LOG_I(TAG, "rx_start: returning %d", (int)protocol_ready);
    subghz_debug_log_write("rx_start: returning %d", (int)protocol_ready);
    return protocol_ready;
}

void subghz_txrx_set_need_save_callback(
    SubGhzTxRx* instance,
    SubGhzTxRxNeedSaveCallback callback,
    void* context) {
    furi_assert(instance);
    instance->need_save_callback = callback;
    instance->need_save_context = context;
}

static void subghz_txrx_tx_stop(SubGhzTxRx* instance) {
    furi_assert(instance);
    furi_assert(instance->txrx_state == SubGhzTxRxStateTx);
    //Stop TX
    subghz_devices_stop_async_tx(instance->radio_device);
    subghz_transmitter_stop(instance->transmitter);
    subghz_transmitter_free(instance->transmitter);

    //if protocol dynamic then we save the last upload
    if(instance->decoder_result->protocol->type == SubGhzProtocolTypeDynamic) {
        if(instance->need_save_callback) {
            instance->need_save_callback(instance->need_save_context);
        }
    }
    subghz_txrx_idle(instance);
    subghz_txrx_speaker_off(instance);
}

FlipperFormat* subghz_txrx_get_fff_data(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->fff_data;
}

SubGhzSetting* subghz_txrx_get_setting(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->setting;
}

void subghz_txrx_stop(SubGhzTxRx* instance) {
    furi_assert(instance);
    FURI_LOG_I(TAG, "txrx_stop: state=%d worker=%p", (int)instance->txrx_state, (void*)instance->worker);

    switch(instance->txrx_state) {
    case SubGhzTxRxStateTx:
        subghz_txrx_tx_stop(instance);
        subghz_txrx_speaker_unmute(instance);
        break;
    case SubGhzTxRxStateRx:
        subghz_txrx_rx_end(instance, true);
        subghz_txrx_speaker_mute(instance);
        break;

    default:
        break;
    }
    FURI_LOG_I(TAG, "txrx_stop: done");
}

void subghz_txrx_hopper_update(SubGhzTxRx* instance, float stay_threshold) {
    furi_assert(instance);

    switch(instance->hopper_state) {
    case SubGhzHopperStateOFF:
    case SubGhzHopperStatePause:
        return;
    case SubGhzHopperStateRSSITimeOut:
        if(instance->hopper_timeout != 0) {
            instance->hopper_timeout--;
            return;
        }
        break;
    default:
        break;
    }
    if(instance->hopper_state != SubGhzHopperStateRSSITimeOut) {
        // See RSSI Calculation timings in CC1101 17.3 RSSI
        float rssi = subghz_devices_get_rssi(instance->radio_device);

        // Stay if RSSI is high enough
        if(rssi > stay_threshold) {
            instance->hopper_timeout = 10;
            instance->hopper_state = SubGhzHopperStateRSSITimeOut;
            return;
        }
    } else {
        instance->hopper_state = SubGhzHopperStateRunning;
    }
    // Select next frequency
    if(instance->hopper_idx_frequency <
       subghz_setting_get_hopper_frequency_count(instance->setting) - 1) {
        instance->hopper_idx_frequency++;
    } else {
        instance->hopper_idx_frequency = 0;
    }

    if(instance->txrx_state == SubGhzTxRxStateRx) {
        subghz_txrx_rx_end(instance, false);
    }
    if(instance->txrx_state == SubGhzTxRxStateIDLE) {
        subghz_receiver_reset(instance->receiver);
        instance->preset->frequency =
            subghz_setting_get_hopper_frequency(instance->setting, instance->hopper_idx_frequency);
        subghz_txrx_rx(instance, instance->preset->frequency);
    }
}

SubGhzHopperState subghz_txrx_hopper_get_state(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->hopper_state;
}

void subghz_txrx_hopper_set_state(SubGhzTxRx* instance, SubGhzHopperState state) {
    furi_assert(instance);
    instance->hopper_state = state;
}

void subghz_txrx_hopper_unpause(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->hopper_state == SubGhzHopperStatePause) {
        instance->hopper_state = SubGhzHopperStateRunning;
    }
}

void subghz_txrx_hopper_pause(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->hopper_state == SubGhzHopperStateRunning) {
        instance->hopper_state = SubGhzHopperStatePause;
    }
}

void subghz_txrx_speaker_on(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_ibutton);
    }

    if(instance->speaker_state == SubGhzSpeakerStateEnable) {
        if(furi_hal_speaker_acquire(30)) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_speaker);
            }
        } else {
            instance->speaker_state = SubGhzSpeakerStateDisable;
        }
    }
}

void subghz_txrx_speaker_off(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
    }
    if(instance->speaker_state != SubGhzSpeakerStateDisable) {
        if(furi_hal_speaker_is_mine()) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
            }
            furi_hal_speaker_release();
            if(instance->speaker_state == SubGhzSpeakerStateShutdown)
                instance->speaker_state = SubGhzSpeakerStateDisable;
        }
    }
}

void subghz_txrx_speaker_mute(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
    }
    if(instance->speaker_state == SubGhzSpeakerStateEnable) {
        if(furi_hal_speaker_is_mine()) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, NULL);
            }
        }
    }
}

void subghz_txrx_speaker_unmute(SubGhzTxRx* instance) {
    furi_assert(instance);
    if(instance->debug_pin_state) {
        subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_ibutton);
    }
    if(instance->speaker_state == SubGhzSpeakerStateEnable) {
        if(furi_hal_speaker_is_mine()) {
            if(!instance->debug_pin_state) {
                subghz_devices_set_async_mirror_pin(instance->radio_device, &gpio_speaker);
            }
        }
    }
}

void subghz_txrx_speaker_set_state(SubGhzTxRx* instance, SubGhzSpeakerState state) {
    furi_assert(instance);
    instance->speaker_state = state;
}

SubGhzSpeakerState subghz_txrx_speaker_get_state(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->speaker_state;
}

bool subghz_txrx_load_decoder_by_name_protocol(SubGhzTxRx* instance, const char* name_protocol) {
    furi_assert(instance);
    furi_assert(name_protocol);
    subghz_txrx_ensure_protocol_plugin(instance);
    bool res = false;
    instance->decoder_result =
        subghz_receiver_search_decoder_base_by_name(instance->receiver, name_protocol);
    if(instance->decoder_result) {
        res = true;
    }
    return res;
}

SubGhzProtocolDecoderBase* subghz_txrx_get_decoder(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->decoder_result;
}

bool subghz_txrx_protocol_is_serializable(SubGhzTxRx* instance) {
    furi_assert(instance);
    return (instance->decoder_result->protocol->flag & SubGhzProtocolFlag_Save) ==
           SubGhzProtocolFlag_Save;
}

bool subghz_txrx_protocol_is_transmittable(SubGhzTxRx* instance, bool check_type) {
    furi_assert(instance);
    const SubGhzProtocol* protocol = instance->decoder_result->protocol;

    if((protocol->flag & SubGhzProtocolFlag_Send) != SubGhzProtocolFlag_Send) {
        return false;
    }
    if(check_type && protocol->type != SubGhzProtocolTypeStatic) {
        return false;
    }

    /* protocol->encoder here belongs to whichever registry is CURRENTLY
     * loaded - and every caller of this function (the Transmitter scene's
     * "can I show a Send button" check, on Saved > Emulate) runs before
     * any TX has actually been attempted, so the only thing loaded so far
     * is one of the RX-only group plugins. Per protocol_groups.h's own
     * comment, those are compiled RX_ONLY (no encoders at all) for every
     * protocol listed in subghz_garage_tx_protocol_map (protocol_groups.c)
     * - which is effectively all of them (Princeton included) - so
     * `protocol->encoder->deserialize` was NULL here unconditionally,
     * reporting every single saved signal as non-transmittable and
     * leaving the Transmitter view with no button to draw and no input
     * handling at all (can_be_sent gates on show_button in
     * views/transmitter.c), even though subghz_txrx_tx_start() would have
     * happily loaded the real per-protocol TX plugin and sent it, the
     * moment a "Send" press could ever have reached it. Ask by protocol
     * NAME whether a dedicated TX plugin exists instead - the exact same
     * lookup subghz_txrx_tx_start() itself uses to route a real send - and
     * only fall back to the loaded encoder pointer for a protocol that
     * isn't in that list (the "generic gen_data_protocol path" automotive
     * protocols such as Kia/VAG/PSA/Ford, which keep a real encoder in
     * every registry they're loaded into and were never affected by this). */
    SubGhzGarageTxProtocol tx_protocol;
    if(subghz_garage_tx_protocol_for_name(protocol->name, &tx_protocol)) {
        return true;
    }
    return protocol->encoder->deserialize != NULL;
}

void subghz_txrx_receiver_set_filter(SubGhzTxRx* instance, SubGhzProtocolFlag filter) {
    furi_assert(instance);
    /* Deliberately does NOT load the protocol plugin - subghz_alloc() calls
     * this unconditionally at app boot, before any scene runs. The value
     * is cached and re-applied once something else actually needs the
     * plugin (see subghz_txrx_ensure_protocol_plugin). */
    instance->receiver_filter = filter;
    instance->receiver_filter_set = true;
    /* instance->receiver doesn't exist yet at boot either (see
     * subghz_txrx_ensure_radio_init) - subghz_txrx_ensure_radio_init()
     * re-applies the cached value above once it does. */
    if(instance->receiver) {
        subghz_receiver_set_filter(instance->receiver, filter);
    }
}

void subghz_txrx_set_rx_callback(
    SubGhzTxRx* instance,
    SubGhzReceiverCallback callback,
    void* context) {
    /* Same reasoning as subghz_txrx_receiver_set_filter - cache only, no
     * eager plugin load, and instance->receiver may not exist yet. */
    instance->rx_callback = callback;
    instance->rx_callback_context = context;
    if(instance->receiver) {
        subghz_receiver_set_rx_callback(instance->receiver, callback, context);
    }
}

void subghz_txrx_set_raw_file_encoder_worker_callback_end(
    SubGhzTxRx* instance,
    SubGhzProtocolEncoderRAWCallbackEnd callback,
    void* context) {
    subghz_protocol_raw_file_encoder_worker_set_callback_end(
        (SubGhzProtocolEncoderRAW*)subghz_transmitter_get_protocol_instance(instance->transmitter),
        callback,
        context);
}

static void subghz_txrx_ensure_external_device(SubGhzTxRx* instance) {
    furi_assert(instance);

    if(instance->external_device_loaded) {
        return;
    }
    instance->external_device_loaded = true;

    FURI_LOG_I(TAG, "Loading external radio device plugin: free heap %zu", memmgr_get_free_heap());
    subghz_garage_devices_load_external();
    FURI_LOG_I(TAG, "External radio device plugin loaded: free heap %zu", memmgr_get_free_heap());
}

bool subghz_txrx_radio_device_is_external_connected(SubGhzTxRx* instance, const char* name) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);

    subghz_txrx_ensure_external_device(instance);

    bool is_connect = false;
    bool is_otg_enabled = furi_hal_power_is_otg_enabled();

    if(!is_otg_enabled) {
        subghz_txrx_radio_device_power_on(instance);
    }

    const SubGhzDevice* device = subghz_devices_get_by_name(name);
    if(device) {
        is_connect = subghz_devices_is_connect(device);
    }

    if(!is_otg_enabled) {
        subghz_txrx_radio_device_power_off(instance);
    }
    return is_connect;
}

SubGhzRadioDeviceType
    subghz_txrx_radio_device_set(SubGhzTxRx* instance, SubGhzRadioDeviceType radio_device_type) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);

    if(radio_device_type == SubGhzRadioDeviceTypeExternalCC1101 &&
       subghz_txrx_radio_device_is_external_connected(instance, SUBGHZ_DEVICE_CC1101_EXT_NAME)) {
        subghz_txrx_radio_device_power_on(instance);
        instance->radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
        subghz_devices_begin(instance->radio_device);
        instance->radio_device_type = SubGhzRadioDeviceTypeExternalCC1101;
    } else {
        subghz_txrx_radio_device_power_off(instance);
        if(instance->radio_device_type != SubGhzRadioDeviceTypeInternal) {
            subghz_devices_end(instance->radio_device);
        }
        instance->radio_device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
        instance->radio_device_type = SubGhzRadioDeviceTypeInternal;
    }

    return instance->radio_device_type;
}

SubGhzRadioDeviceType subghz_txrx_radio_device_get(SubGhzTxRx* instance) {
    furi_assert(instance);
    /* Unlike every other accessor here, this one was missing the ensure_
     * radio_init() call - so the very first query of a fresh session (the
     * Read/Read RAW Start screen's status-bar label, drawn before RX has
     * ever begun) returned the boot-time placeholder set in
     * subghz_txrx_alloc() (always Internal) instead of the real detected
     * type. Cosmetic only - rx_start() itself already reaches ensure_
     * radio_init() via ensure_protocol_plugin() before actually picking a
     * device - but it made the external-CC1101 status icon lie for the
     * whole first Read session after every launch. */
    subghz_txrx_ensure_radio_init(instance);
    return instance->radio_device_type;
}

float subghz_txrx_radio_device_get_rssi(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);
    return subghz_devices_get_rssi(instance->radio_device);
}

const char* subghz_txrx_radio_device_get_name(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);
    return subghz_devices_get_name(instance->radio_device);
}

bool subghz_txrx_radio_device_is_frequency_valid(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);
    return subghz_devices_is_frequency_valid(instance->radio_device, frequency);
}

bool subghz_txrx_radio_device_is_tx_allowed(SubGhzTxRx* instance, uint32_t frequency) {
    // TODO: Remake this function to check if the frequency is allowed on specific module - for modules not based on CC1101
    furi_assert(instance);
    UNUSED(frequency);
    /*
    furi_assert(instance->txrx_state != SubGhzTxRxStateSleep);

    subghz_devices_idle(instance->radio_device);
    subghz_devices_set_frequency(instance->radio_device, frequency);

    bool ret = subghz_devices_set_tx(instance->radio_device);
    subghz_devices_idle(instance->radio_device);

    return ret;
    */
    return true;
}

void subghz_txrx_set_debug_pin_state(SubGhzTxRx* instance, bool state) {
    furi_assert(instance);
    instance->debug_pin_state = state;
}

bool subghz_txrx_get_debug_pin_state(SubGhzTxRx* instance) {
    furi_assert(instance);
    return instance->debug_pin_state;
}

void subghz_txrx_reset_dynamic_and_custom_btns(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_ensure_radio_init(instance);
    subghz_garage_env_reset_keeloq(instance->environment);

    if(instance->protocol_plugin && instance->protocol_plugin->faac_slh_reset_prog_mode) {
        instance->protocol_plugin->faac_slh_reset_prog_mode();
    }

    subghz_custom_btns_reset();
}

SubGhzReceiver* subghz_txrx_get_receiver(SubGhzTxRx* instance) {
    furi_assert(instance);
    subghz_txrx_ensure_protocol_plugin(instance);
    return instance->receiver;
}

void subghz_txrx_set_default_preset(SubGhzTxRx* instance, uint32_t frequency) {
    furi_assert(instance);

    const char* default_modulation = "AM650";
    if(frequency == 0) {
        frequency = subghz_setting_get_default_frequency(subghz_txrx_get_setting(instance));
    }
    subghz_txrx_set_preset(instance, default_modulation, frequency, NULL, 0);
}

const char* subghz_txrx_set_preset_internal(
    SubGhzTxRx* instance,
    uint32_t frequency,
    uint8_t index,
    uint8_t tx_power) {
    furi_assert(instance);

    //Grab the prset name.
    SubGhzSetting* setting = subghz_txrx_get_setting(instance);
    const char* preset_name = subghz_setting_get_preset_name(setting, index);
    subghz_garage_setting_mark_default_frequency(setting, frequency);

    //Get the preset data now so we can set TX power.
    uint8_t* preset_data = subghz_setting_get_preset_data(setting, index);
    size_t preset_data_size = subghz_setting_get_preset_data_size(setting, index);

    //Edit TX power, if necessary.
    subghz_txrx_set_tx_power(preset_data, preset_data_size, tx_power);

    //Set the Updated Preset.
    subghz_txrx_set_preset(instance, preset_name, frequency, preset_data, preset_data_size);

    return preset_name;
}
