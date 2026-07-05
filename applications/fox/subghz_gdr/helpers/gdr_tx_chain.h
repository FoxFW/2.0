// helpers/gdr_tx_chain.h
#pragma once

#include "gdr_types.h"

#ifdef ENABLE_SHIELD_RX_SCENE

#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/devices/devices.h>

#include "radio_device_loader.h"

typedef struct {
    const SubGhzDevice* device;
    const GpioPin* data_gpio;
    uint8_t* preset_data;
    size_t preset_data_size;
    FuriString* preset_name;
    uint32_t frequency;
    GDRTxRxState state;
} GDRTxChain;

GDRTxChain* gdr_tx_chain_alloc(void);
void gdr_tx_chain_free(GDRTxChain* chain);

bool gdr_tx_chain_acquire_device(GDRTxChain* chain);

bool gdr_tx_chain_configure(
    GDRTxChain* chain,
    SubGhzSetting* setting,
    uint32_t rx_frequency,
    int32_t offset_hz,
    uint8_t tx_power);

bool gdr_tx_chain_start_carrier(GDRTxChain* chain);
void gdr_tx_chain_stop(GDRTxChain* chain);

#endif // ENABLE_SHIELD_RX_SCENE
