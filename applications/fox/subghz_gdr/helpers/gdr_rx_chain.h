// helpers/gdr_rx_chain.h
#pragma once

#include "gdr_types.h"

#if defined(ENABLE_DUAL_RX_SCENE) || defined(ENABLE_SHIELD_RX_SCENE)

#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/types.h>
#include <lib/flipper_application/plugins/plugin_manager.h>
#include <lib/flipper_application/plugins/composite_resolver.h>

#include "radio_device_loader.h"
#include "../protocols/protocol_items.h"
#include "../protocols/gdr_protocol_plugins.h"

typedef struct {
    char label; // 'A' or 'B' (display tag)
    const SubGhzDevice* device;
    bool is_external;

    SubGhzWorker* worker;
    SubGhzReceiver* receiver;
    SubGhzEnvironment* environment;

    CompositeApiResolver* resolver;
    PluginManager* plugin_manager;
    const GDRProtocolPlugin* plugin;
    const SubGhzProtocolRegistry* registry;
    GDRProtocolRegistryFilter filter;

    SubGhzRadioPreset preset; // .name is an owned FuriString
    uint8_t* base_preset_data;
    size_t base_preset_data_size;

    uint8_t* owned_preset_data;
    uint32_t frequency;
    uint32_t rx_bandwidth_hz;

    GDRTxRxState state;
} GDRRxChain;

GDRRxChain* gdr_rx_chain_alloc(char label);

void gdr_rx_chain_free(GDRRxChain* chain);

bool gdr_rx_chain_acquire_device(
    GDRRxChain* chain,
    SubGhzRadioDeviceType type);

bool gdr_rx_chain_set_preset(
    GDRRxChain* chain,
    SubGhzSetting* setting,
    const char* preset_name,
    uint32_t frequency);

bool gdr_rx_chain_set_preset_data(
    GDRRxChain* chain,
    const char* preset_name,
    uint8_t* preset_data,
    size_t preset_data_size,
    uint32_t frequency);

bool gdr_rx_chain_apply_shield_profile(GDRRxChain* chain);

bool gdr_rx_chain_init_receiver(GDRRxChain* chain);

void gdr_rx_chain_set_decode_callback(
    GDRRxChain* chain,
    SubGhzReceiverCallback callback,
    void* context);

bool gdr_rx_chain_start(GDRRxChain* chain);

void gdr_rx_chain_stop(GDRRxChain* chain);

float gdr_rx_chain_get_rssi(GDRRxChain* chain);

#endif // ENABLE_DUAL_RX_SCENE || ENABLE_SHIELD_RX_SCENE
