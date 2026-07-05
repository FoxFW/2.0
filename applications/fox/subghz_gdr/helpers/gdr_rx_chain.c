// helpers/gdr_rx_chain.c
#include "gdr_rx_chain.h"

#if defined(ENABLE_DUAL_RX_SCENE) || defined(ENABLE_SHIELD_RX_SCENE)

#include <furi.h>
#include <loader/firmware_api/firmware_api.h>
#include "../protocols/keys.h"

#define TAG "GDRRxChain"

#define GDR_CHAIN_KEYSTORE_DIR APP_ASSETS_PATH("encrypted")

#define GDR_CC1101_REG_FIFOTHR  0x03U
#define GDR_CC1101_REG_FSCTRL1  0x07U
#define GDR_CC1101_REG_MDMCFG4  0x10U
#define GDR_CC1101_REG_MDMCFG3  0x11U
#define GDR_CC1101_REG_MDMCFG2  0x12U
#define GDR_CC1101_REG_DEVIATN  0x15U
#define GDR_CC1101_REG_AGCCTRL2 0x1BU
#define GDR_CC1101_REG_AGCCTRL1 0x1CU
#define GDR_CC1101_REG_AGCCTRL0 0x1DU
#define GDR_CC1101_REG_FREND1   0x21U
#define GDR_CC1101_REG_TEST2    0x2CU
#define GDR_CC1101_REG_TEST1    0x2DU

#define GDR_CC1101_CHANBW_135_KHZ_MASK 0xA0U
#define GDR_CC1101_XTAL_HZ             26000000UL
#define GDR_FM_BANDWIDTH_GUARD_HZ      50000UL

#define GDR_CC1101_MOD_FORMAT_MASK 0x70U
#define GDR_CC1101_MOD_FORMAT_2FSK 0x00U
#define GDR_CC1101_MOD_FORMAT_GFSK 0x10U
#define GDR_CC1101_MOD_FORMAT_OOK  0x30U

static bool gdr_rx_chain_preset_get_register(
    const uint8_t* data,
    size_t size,
    uint8_t reg,
    uint8_t* value) {
    if(!data || !value || (size < 2U)) {
        return false;
    }
    for(size_t i = 0; i + 1U < size; i += 2U) {
        if((data[i] == 0x00U) && (data[i + 1U] == 0x00U)) {
            break;
        }
        if(data[i] == reg) {
            *value = data[i + 1U];
            return true;
        }
    }
    return false;
}

static bool gdr_rx_chain_preset_set_register(
    uint8_t* data,
    size_t size,
    uint8_t reg,
    uint8_t value) {
    if(!data || (size < 2U)) {
        return false;
    }
    for(size_t i = 0; i + 1U < size; i += 2U) {
        if((data[i] == 0x00U) && (data[i + 1U] == 0x00U)) {
            break;
        }
        if(data[i] == reg) {
            data[i + 1U] = value;
            return true;
        }
    }
    return false;
}

static bool gdr_rx_chain_preset_find_terminator(
    const uint8_t* data,
    size_t size,
    size_t* offset) {
    if(!data || !offset || size < 2U) {
        return false;
    }
    for(size_t i = 0; i + 1U < size; i += 2U) {
        if(data[i] == 0x00U && data[i + 1U] == 0x00U) {
            *offset = i;
            return true;
        }
    }
    return false;
}

static uint32_t gdr_rx_chain_channel_bandwidth_hz(uint8_t mdmcfg4) {
    uint8_t exponent = (mdmcfg4 >> 6U) & 0x03U;
    uint8_t mantissa = (mdmcfg4 >> 4U) & 0x03U;
    uint32_t denominator = 8UL * (4UL + mantissa) * (1UL << exponent);
    return GDR_CC1101_XTAL_HZ / denominator;
}

static uint32_t gdr_rx_chain_data_rate_hz(uint8_t mdmcfg4, uint8_t mdmcfg3) {
    uint8_t exponent = mdmcfg4 & 0x0FU;
    uint64_t numerator =
        (uint64_t)(256UL + mdmcfg3) * (1ULL << exponent) * GDR_CC1101_XTAL_HZ;
    return (uint32_t)((numerator + (1ULL << 27U)) >> 28U);
}

static uint32_t gdr_rx_chain_deviation_hz(uint8_t deviatn) {
    uint8_t exponent = (deviatn >> 4U) & 0x07U;
    uint8_t mantissa = deviatn & 0x07U;
    uint64_t numerator =
        (uint64_t)GDR_CC1101_XTAL_HZ * (8UL + mantissa) * (1ULL << exponent);
    return (uint32_t)((numerator + (1ULL << 16U)) >> 17U);
}

static uint8_t gdr_rx_chain_select_bandwidth_bits(
    uint32_t minimum_hz,
    uint32_t* selected_hz) {
    static const uint8_t bandwidth_bits[] = {
        0x0FU,
        0x0EU,
        0x0DU,
        0x0CU,
        0x0BU,
        0x0AU,
        0x09U,
        0x08U,
        0x07U,
        0x06U,
        0x05U,
        0x04U,
        0x03U,
        0x02U,
        0x01U,
        0x00U,
    };

    const size_t bandwidth_count = sizeof(bandwidth_bits) / sizeof(bandwidth_bits[0]);
    for(size_t i = 0; i < bandwidth_count; i++) {
        uint32_t bandwidth =
            gdr_rx_chain_channel_bandwidth_hz((uint8_t)(bandwidth_bits[i] << 4U));
        if(bandwidth >= minimum_hz) {
            if(selected_hz) {
                *selected_hz = bandwidth;
            }
            return bandwidth_bits[i];
        }
    }

    if(selected_hz) {
        *selected_hz = gdr_rx_chain_channel_bandwidth_hz(0x00U);
    }
    return 0x00U;
}

static const char* gdr_rx_chain_plugin_path(GDRProtocolRegistryFilter filter) {
    return (filter == GDRProtocolRegistryFilterFM) ?
               APP_ASSETS_PATH("plugins/gdr_fm_plugin.fal") :
               APP_ASSETS_PATH("plugins/gdr_am_plugin.fal");
}

GDRRxChain* gdr_rx_chain_alloc(char label) {
    GDRRxChain* chain = malloc(sizeof(GDRRxChain));
    furi_check(chain);
    memset(chain, 0, sizeof(GDRRxChain));
    chain->label = label;
    chain->preset.name = furi_string_alloc();
    furi_check(chain->preset.name);
    chain->state = GDRTxRxStateIDLE;
    chain->filter = GDRProtocolRegistryFilterAM;
    return chain;
}

static void gdr_rx_chain_unload_plugin(GDRRxChain* chain) {
    chain->plugin = NULL;
    chain->registry = NULL;
    if(chain->plugin_manager) {
        plugin_manager_free(chain->plugin_manager);
        chain->plugin_manager = NULL;
    }
    if(chain->resolver) {
        composite_api_resolver_free(chain->resolver);
        chain->resolver = NULL;
    }
}

void gdr_rx_chain_free(GDRRxChain* chain) {
    if(!chain) {
        return;
    }

    // Make sure RX is stopped before tearing anything down.
    gdr_rx_chain_stop(chain);

    if(chain->receiver) {
        subghz_receiver_set_rx_callback(chain->receiver, NULL, NULL);
        subghz_receiver_free(chain->receiver);
        chain->receiver = NULL;
    }

    if(chain->worker) {
        if(subghz_worker_is_running(chain->worker)) {
            subghz_worker_stop(chain->worker);
        }
        subghz_worker_free(chain->worker);
        chain->worker = NULL;
    }

    if(chain->device) {
        subghz_devices_idle(chain->device);
        radio_device_loader_end(chain->device);
        chain->device = NULL;
    }

    gdr_rx_chain_unload_plugin(chain);

    if(chain->environment) {
        subghz_environment_free(chain->environment);
        chain->environment = NULL;
    }

    if(chain->preset.name) {
        furi_string_free(chain->preset.name);
        chain->preset.name = NULL;
    }

    if(chain->owned_preset_data) {
        free(chain->owned_preset_data);
        chain->owned_preset_data = NULL;
    }

    free(chain);
}

bool gdr_rx_chain_acquire_device(
    GDRRxChain* chain,
    SubGhzRadioDeviceType type) {
    furi_check(chain);

    chain->device = radio_device_loader_set(NULL, type);
    if(!chain->device) {
        FURI_LOG_E(TAG, "[%c] Failed to acquire radio device (type=%d)", chain->label, type);
        return false;
    }

    chain->is_external = radio_device_loader_is_external(chain->device);

    if(type == SubGhzRadioDeviceTypeExternalCC1101 && !chain->is_external) {

        FURI_LOG_E(TAG, "[%c] External requested but unavailable", chain->label);
        radio_device_loader_end(chain->device);
        chain->device = NULL;
        return false;
    }

    subghz_devices_reset(chain->device);
    subghz_devices_idle(chain->device);
    FURI_LOG_I(
        TAG, "[%c] Acquired %s radio", chain->label, chain->is_external ? "external" : "internal");
    return true;
}

bool gdr_rx_chain_set_preset(
    GDRRxChain* chain,
    SubGhzSetting* setting,
    const char* preset_name,
    uint32_t frequency) {
    furi_check(chain);
    furi_check(setting);
    furi_check(preset_name);

    size_t preset_count = subghz_setting_get_preset_count(setting);
    for(size_t i = 0; i < preset_count; i++) {
        if(strcmp(subghz_setting_get_preset_name(setting, i), preset_name) == 0) {
            return gdr_rx_chain_set_preset_data(
                chain,
                preset_name,
                subghz_setting_get_preset_data(setting, i),
                subghz_setting_get_preset_data_size(setting, i),
                frequency);
        }
    }

    FURI_LOG_E(TAG, "[%c] Unknown preset %s", chain->label, preset_name);
    return false;
}

bool gdr_rx_chain_set_preset_data(
    GDRRxChain* chain,
    const char* preset_name,
    uint8_t* preset_data,
    size_t preset_data_size,
    uint32_t frequency) {
    furi_check(chain);
    furi_check(preset_name);

    if(!preset_data || preset_data_size < 2U) {
        FURI_LOG_E(TAG, "[%c] Invalid preset data for %s", chain->label, preset_name);
        return false;
    }

    if(chain->owned_preset_data) {
        free(chain->owned_preset_data);
        chain->owned_preset_data = NULL;
    }

    furi_string_set(chain->preset.name, preset_name);
    chain->preset.frequency = frequency;
    chain->preset.data = preset_data;
    chain->preset.data_size = preset_data_size;
    chain->base_preset_data = preset_data;
    chain->base_preset_data_size = preset_data_size;
    chain->frequency = frequency;
    chain->filter =
        gdr_get_protocol_registry_filter_for_preset(preset_data, preset_data_size);
    uint8_t mdmcfg4 = 0U;
    chain->rx_bandwidth_hz = gdr_rx_chain_preset_get_register(
                                 preset_data,
                                 preset_data_size,
                                 GDR_CC1101_REG_MDMCFG4,
                                 &mdmcfg4) ?
                                 gdr_rx_chain_channel_bandwidth_hz(mdmcfg4) :
                                 0U;
    return true;
}

static bool gdr_rx_chain_apply_ook_shield_profile(GDRRxChain* chain) {
    if(!chain->base_preset_data || chain->preset.data_size < 2U) {
        FURI_LOG_E(TAG, "[%c] cannot narrow RX BW without preset data", chain->label);
        return false;
    }

    uint8_t mdmcfg4 = 0U;
    if(!gdr_rx_chain_preset_get_register(
           chain->base_preset_data,
           chain->base_preset_data_size,
           GDR_CC1101_REG_MDMCFG4,
           &mdmcfg4)) {
        FURI_LOG_W(TAG, "[%c] OOK preset missing MDMCFG4; retaining original", chain->label);
        return true;
    }

    const uint8_t narrowed_mdmcfg4 =
        (uint8_t)((mdmcfg4 & 0x0FU) | GDR_CC1101_CHANBW_135_KHZ_MASK);

    const struct {
        uint8_t reg;
        uint8_t value;
    } narrow_registers[] = {
        {GDR_CC1101_REG_FIFOTHR, 0x47U},
        {GDR_CC1101_REG_FSCTRL1, 0x06U},
        {GDR_CC1101_REG_MDMCFG4, narrowed_mdmcfg4},
        {GDR_CC1101_REG_AGCCTRL2, 0x04U},
        {GDR_CC1101_REG_AGCCTRL1, 0x00U},
        {GDR_CC1101_REG_AGCCTRL0, 0x92U},
        {GDR_CC1101_REG_FREND1, 0x56U},
        {GDR_CC1101_REG_TEST2, 0x81U},
        {GDR_CC1101_REG_TEST1, 0x35U},
    };

    const size_t register_count = sizeof(narrow_registers) / sizeof(narrow_registers[0]);
    size_t missing_count = 0U;
    for(size_t i = 0; i < register_count; i++) {
        uint8_t value = 0U;
        if(!gdr_rx_chain_preset_get_register(
               chain->base_preset_data,
               chain->base_preset_data_size,
               narrow_registers[i].reg,
               &value)) {
            missing_count++;
        }
    }

    size_t terminator_offset = 0U;
    if(!gdr_rx_chain_preset_find_terminator(
           chain->base_preset_data, chain->base_preset_data_size, &terminator_offset)) {
        FURI_LOG_W(TAG, "[%c] OOK preset has no terminator; retaining original", chain->label);
        return true;
    }

    const size_t expanded_size = chain->base_preset_data_size + (missing_count * 2U);
    uint8_t* copy = malloc(expanded_size);
    if(!copy) {
        FURI_LOG_E(TAG, "[%c] failed to build narrow RX preset", chain->label);
        return false;
    }

    memcpy(copy, chain->base_preset_data, terminator_offset);
    size_t write_offset = terminator_offset;
    for(size_t i = 0; i < register_count; i++) {
        uint8_t value = 0U;
        if(!gdr_rx_chain_preset_get_register(
               chain->base_preset_data,
               chain->base_preset_data_size,
               narrow_registers[i].reg,
               &value)) {
            copy[write_offset++] = narrow_registers[i].reg;
            copy[write_offset++] = narrow_registers[i].value;
        }
    }
    memcpy(
        &copy[write_offset],
        &chain->base_preset_data[terminator_offset],
        chain->base_preset_data_size - terminator_offset);

    for(size_t i = 0; i < register_count; i++) {
        if(!gdr_rx_chain_preset_set_register(
               copy, expanded_size, narrow_registers[i].reg, narrow_registers[i].value)) {
            FURI_LOG_E(TAG, "[%c] failed to patch narrow RX preset", chain->label);
            free(copy);
            return false;
        }
    }

    chain->owned_preset_data = copy;
    chain->preset.data = copy;
    chain->preset.data_size = expanded_size;
    chain->rx_bandwidth_hz =
        gdr_rx_chain_channel_bandwidth_hz(GDR_CC1101_CHANBW_135_KHZ_MASK);
    FURI_LOG_I(TAG, "[%c] applied TI 135 kHz OOK sensitivity profile", chain->label);
    return true;
}

static bool gdr_rx_chain_apply_fm_shield_profile(GDRRxChain* chain) {
    uint8_t mdmcfg2 = 0U;
    uint8_t mdmcfg3 = 0U;
    uint8_t mdmcfg4 = 0U;
    uint8_t deviatn = 0U;
    if(!gdr_rx_chain_preset_get_register(
           chain->base_preset_data,
           chain->base_preset_data_size,
           GDR_CC1101_REG_MDMCFG2,
           &mdmcfg2) ||
       !gdr_rx_chain_preset_get_register(
           chain->base_preset_data,
           chain->base_preset_data_size,
           GDR_CC1101_REG_MDMCFG3,
           &mdmcfg3) ||
       !gdr_rx_chain_preset_get_register(
           chain->base_preset_data,
           chain->base_preset_data_size,
           GDR_CC1101_REG_MDMCFG4,
           &mdmcfg4) ||
       !gdr_rx_chain_preset_get_register(
           chain->base_preset_data,
           chain->base_preset_data_size,
           GDR_CC1101_REG_DEVIATN,
           &deviatn)) {
        FURI_LOG_W(TAG, "[%c] incomplete FM preset; retaining original bandwidth", chain->label);
        return true;
    }

    uint8_t modulation = mdmcfg2 & GDR_CC1101_MOD_FORMAT_MASK;
    if(modulation != GDR_CC1101_MOD_FORMAT_2FSK &&
       modulation != GDR_CC1101_MOD_FORMAT_GFSK) {
        FURI_LOG_W(TAG, "[%c] unsupported FM format; retaining original bandwidth", chain->label);
        return true;
    }

    uint32_t data_rate = gdr_rx_chain_data_rate_hz(mdmcfg4, mdmcfg3);
    uint32_t deviation = gdr_rx_chain_deviation_hz(deviatn);
    uint32_t minimum_bandwidth =
        data_rate + (2UL * deviation) + GDR_FM_BANDWIDTH_GUARD_HZ;
    uint32_t selected_bandwidth = 0U;
    uint8_t bandwidth_bits =
        gdr_rx_chain_select_bandwidth_bits(minimum_bandwidth, &selected_bandwidth);
    uint32_t original_bandwidth = gdr_rx_chain_channel_bandwidth_hz(mdmcfg4);

    if(selected_bandwidth >= original_bandwidth) {
        chain->rx_bandwidth_hz = original_bandwidth;
        FURI_LOG_I(
            TAG,
            "[%c] FM preset already uses suitable %lu Hz bandwidth",
            chain->label,
            original_bandwidth);
        return true;
    }

    uint8_t* copy = malloc(chain->base_preset_data_size);
    if(!copy) {
        FURI_LOG_E(TAG, "[%c] failed to copy FM preset", chain->label);
        return false;
    }
    memcpy(copy, chain->base_preset_data, chain->base_preset_data_size);

    uint8_t profiled_mdmcfg4 = (uint8_t)((mdmcfg4 & 0x0FU) | (bandwidth_bits << 4U));
    if(!gdr_rx_chain_preset_set_register(
           copy,
           chain->base_preset_data_size,
           GDR_CC1101_REG_MDMCFG4,
           profiled_mdmcfg4)) {
        free(copy);
        return true;
    }

    if(selected_bandwidth <= 101562UL) {
        uint8_t agcctrl2 = 0U;
        if(gdr_rx_chain_preset_get_register(
               copy,
               chain->base_preset_data_size,
               GDR_CC1101_REG_AGCCTRL2,
               &agcctrl2)) {
            gdr_rx_chain_preset_set_register(
                copy,
                chain->base_preset_data_size,
                GDR_CC1101_REG_AGCCTRL2,
                (uint8_t)((agcctrl2 & 0xF8U) | 0x03U));
        }
    }

    chain->owned_preset_data = copy;
    chain->preset.data = copy;
    chain->preset.data_size = chain->base_preset_data_size;
    chain->rx_bandwidth_hz = selected_bandwidth;
    FURI_LOG_I(
        TAG,
        "[%c] FM Shield profile: rate=%lu dev=%lu bandwidth=%lu Hz",
        chain->label,
        data_rate,
        deviation,
        selected_bandwidth);
    return true;
}

bool gdr_rx_chain_apply_shield_profile(GDRRxChain* chain) {
    furi_check(chain);

    if(chain->owned_preset_data) {
        free(chain->owned_preset_data);
        chain->owned_preset_data = NULL;
    }
    chain->preset.data = chain->base_preset_data;
    chain->preset.data_size = chain->base_preset_data_size;

    uint8_t mdmcfg2 = 0U;
    if(!gdr_rx_chain_preset_get_register(
           chain->base_preset_data,
           chain->base_preset_data_size,
           GDR_CC1101_REG_MDMCFG2,
           &mdmcfg2)) {
        FURI_LOG_W(TAG, "[%c] preset modulation unknown; retaining original", chain->label);
        return true;
    }

    switch(mdmcfg2 & GDR_CC1101_MOD_FORMAT_MASK) {
    case GDR_CC1101_MOD_FORMAT_OOK:
        return gdr_rx_chain_apply_ook_shield_profile(chain);
    case GDR_CC1101_MOD_FORMAT_2FSK:
    case GDR_CC1101_MOD_FORMAT_GFSK:
        return gdr_rx_chain_apply_fm_shield_profile(chain);
    default:
        FURI_LOG_W(TAG, "[%c] no Shield profile for modulation; retaining original", chain->label);
        return true;
    }
}

bool gdr_rx_chain_init_receiver(GDRRxChain* chain) {
    furi_check(chain);

    if(!chain->environment) {
        chain->environment = subghz_environment_alloc();
        if(!chain->environment) {
            FURI_LOG_E(TAG, "[%c] Failed to allocate environment", chain->label);
            return false;
        }
    }

    if(!chain->plugin) {
        CompositeApiResolver* resolver = composite_api_resolver_alloc();
        if(!resolver) {
            FURI_LOG_E(TAG, "[%c] Failed to allocate plugin resolver", chain->label);
            return false;
        }
        composite_api_resolver_add(resolver, firmware_api_interface);

        PluginManager* manager = plugin_manager_alloc(
            GDR_PROTOCOL_PLUGIN_APP_ID,
            GDR_PROTOCOL_PLUGIN_API_VERSION,
            composite_api_resolver_get(resolver));
        if(!manager) {
            FURI_LOG_E(TAG, "[%c] Failed to allocate plugin manager", chain->label);
            composite_api_resolver_free(resolver);
            return false;
        }

        const char* plugin_path = gdr_rx_chain_plugin_path(chain->filter);
        PluginManagerError error = plugin_manager_load_single(manager, plugin_path);
        if(error != PluginManagerErrorNone) {
            FURI_LOG_E(TAG, "[%c] Failed to load plugin %s: %d", chain->label, plugin_path, (int)error);
            plugin_manager_free(manager);
            composite_api_resolver_free(resolver);
            return false;
        }

        const GDRProtocolPlugin* plugin = plugin_manager_get_ep(manager, 0U);
        if(!plugin || !plugin->registry || plugin->filter != chain->filter) {
            FURI_LOG_E(TAG, "[%c] Invalid plugin entry point", chain->label);
            plugin_manager_free(manager);
            composite_api_resolver_free(resolver);
            return false;
        }

        chain->resolver = resolver;
        chain->plugin_manager = manager;
        chain->plugin = plugin;
        chain->registry = plugin->registry;
    }

    subghz_environment_set_protocol_registry(chain->environment, chain->registry);

    subghz_environment_load_keystore(chain->environment, GDR_CHAIN_KEYSTORE_DIR);
    gdr_keys_load(chain->environment);

    if(!chain->receiver) {
        chain->receiver = subghz_receiver_alloc_init(chain->environment);
        if(!chain->receiver) {
            FURI_LOG_E(TAG, "[%c] Failed to allocate receiver", chain->label);
            return false;
        }
        subghz_receiver_set_filter(chain->receiver, SubGhzProtocolFlag_Decodable);
    }

    if(!chain->worker) {
        chain->worker = subghz_worker_alloc();
        if(!chain->worker) {
            FURI_LOG_E(TAG, "[%c] Failed to allocate worker", chain->label);
            return false;
        }
        subghz_worker_set_overrun_callback(
            chain->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
        subghz_worker_set_pair_callback(
            chain->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
        subghz_worker_set_context(chain->worker, chain->receiver);
    }

    return true;
}

void gdr_rx_chain_set_decode_callback(
    GDRRxChain* chain,
    SubGhzReceiverCallback callback,
    void* context) {
    furi_check(chain);
    furi_check(chain->receiver);
    subghz_receiver_set_rx_callback(chain->receiver, callback, context);
}

bool gdr_rx_chain_start(GDRRxChain* chain) {
    furi_check(chain);
    if(!chain->device || !chain->worker || !chain->receiver) {
        FURI_LOG_E(TAG, "[%c] start rejected (incomplete stack)", chain->label);
        return false;
    }
    if(chain->state == GDRTxRxStateRx) {
        return true;
    }

    if(!subghz_devices_is_frequency_valid(chain->device, chain->frequency)) {
        FURI_LOG_E(TAG, "[%c] invalid frequency %lu", chain->label, chain->frequency);
        return false;
    }

    subghz_receiver_reset(chain->receiver);

    subghz_devices_reset(chain->device);
    subghz_devices_idle(chain->device);
    subghz_devices_load_preset(chain->device, FuriHalSubGhzPresetCustom, chain->preset.data);
    subghz_devices_set_frequency(chain->device, chain->frequency);
    subghz_devices_flush_rx(chain->device);
    subghz_devices_set_rx(chain->device);

    subghz_devices_start_async_rx(chain->device, subghz_worker_rx_callback, chain->worker);
    subghz_worker_start(chain->worker);
    chain->state = GDRTxRxStateRx;
    FURI_LOG_I(TAG, "[%c] RX started on %lu Hz", chain->label, chain->frequency);
    return true;
}

void gdr_rx_chain_stop(GDRRxChain* chain) {
    if(!chain) {
        return;
    }
    if(chain->state != GDRTxRxStateRx) {
        return;
    }

    if(chain->worker && subghz_worker_is_running(chain->worker)) {
        subghz_worker_stop(chain->worker);
    }
    if(chain->device) {
        subghz_devices_stop_async_rx(chain->device);
        subghz_devices_idle(chain->device);
    }
    chain->state = GDRTxRxStateIDLE;
}

float gdr_rx_chain_get_rssi(GDRRxChain* chain) {
    furi_check(chain);
    if(!chain->device || chain->state != GDRTxRxStateRx) {
        return -127.0f;
    }
    return subghz_devices_get_rssi(chain->device);
}

#endif // ENABLE_DUAL_RX_SCENE || ENABLE_SHIELD_RX_SCENE
