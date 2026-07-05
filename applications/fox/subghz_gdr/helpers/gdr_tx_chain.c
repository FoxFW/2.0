// helpers/gdr_tx_chain.c
#include "gdr_tx_chain.h"

#ifdef ENABLE_SHIELD_RX_SCENE

#include <furi.h>
#include <string.h>

#define TAG "GDRTxChain"

#define GDR_TX_CARRIER_PRESET      "AM650"
#define GDR_TX_POWER_COUNT         9U
#define GDR_TX_PRESET_VALUES_AM    8U
#define GDR_TX_PRESET_VALUES_COUNT 17U

static const uint8_t gdr_tx_power_value[GDR_TX_PRESET_VALUES_COUNT] = {
    0,
    0xC0,
    0xC8,
    0x84,
    0x60,
    0x34,
    0x1D,
    0x0E,
    0x12,
    0xC0,
    0xCD,
    0x86,
    0x50,
    0x26,
    0x1D,
    0x17,
    0x03,
};

static size_t
    gdr_tx_chain_get_pa_table_offset(const uint8_t* preset_data, size_t preset_size) {
    size_t offset = 0;
    while((offset + 1U) < preset_size) {
        if(preset_data[offset] == 0U) {
            return (offset + 2U) < preset_size ? offset + 2U : 0U;
        }
        offset += 2U;
    }
    return 0U;
}

static void gdr_tx_chain_apply_tx_power(
    uint8_t* preset_data,
    size_t preset_size,
    uint8_t tx_power) {
    if(!tx_power || tx_power >= GDR_TX_POWER_COUNT) {
        return;
    }

    const size_t pa_offset = gdr_tx_chain_get_pa_table_offset(preset_data, preset_size);
    if(!pa_offset) {
        return;
    }

    const uint8_t fm_byte = preset_data[pa_offset];
    const uint8_t am_byte = preset_data[pa_offset + 1U];

    if(fm_byte && am_byte) {
        return;
    }
    if(fm_byte) {
        preset_data[pa_offset] = gdr_tx_power_value[tx_power];
    } else if(am_byte) {
        preset_data[pa_offset + 1U] =
            gdr_tx_power_value[GDR_TX_PRESET_VALUES_AM + tx_power];
    }
}

static uint8_t*
    gdr_tx_chain_copy_preset(const uint8_t* src, size_t src_size, size_t* out_size) {
    if(!src || !src_size || !out_size) {
        return NULL;
    }

    uint8_t* copy = malloc(src_size);
    if(!copy) {
        return NULL;
    }

    memcpy(copy, src, src_size);
    *out_size = src_size;
    return copy;
}

GDRTxChain* gdr_tx_chain_alloc(void) {
    GDRTxChain* chain = malloc(sizeof(GDRTxChain));
    furi_check(chain);
    memset(chain, 0, sizeof(GDRTxChain));
    chain->preset_name = furi_string_alloc();
    furi_check(chain->preset_name);
    chain->state = GDRTxRxStateIDLE;
    return chain;
}

void gdr_tx_chain_free(GDRTxChain* chain) {
    if(!chain) {
        return;
    }

    gdr_tx_chain_stop(chain);

    if(chain->device) {
        subghz_devices_idle(chain->device);
        radio_device_loader_end(chain->device);
        chain->device = NULL;
    }

    if(chain->preset_data) {
        free(chain->preset_data);
        chain->preset_data = NULL;
        chain->preset_data_size = 0;
    }

    if(chain->preset_name) {
        furi_string_free(chain->preset_name);
        chain->preset_name = NULL;
    }

    free(chain);
}

bool gdr_tx_chain_acquire_device(GDRTxChain* chain) {
    furi_check(chain);

    chain->device = radio_device_loader_set(NULL, SubGhzRadioDeviceTypeInternal);
    if(!chain->device) {
        FURI_LOG_E(TAG, "Failed to acquire internal radio");
        return false;
    }

    if(radio_device_loader_is_external(chain->device)) {
        FURI_LOG_E(TAG, "Internal radio requested but external was acquired");
        radio_device_loader_end(chain->device);
        chain->device = NULL;
        return false;
    }

    subghz_devices_reset(chain->device);
    subghz_devices_idle(chain->device);
    chain->data_gpio = subghz_devices_get_data_gpio(chain->device);
    if(!chain->data_gpio) {
        FURI_LOG_E(TAG, "Internal radio has no data GPIO");
        radio_device_loader_end(chain->device);
        chain->device = NULL;
        return false;
    }
    FURI_LOG_I(TAG, "Acquired internal radio for carrier TX");
    return true;
}

bool gdr_tx_chain_configure(
    GDRTxChain* chain,
    SubGhzSetting* setting,
    uint32_t rx_frequency,
    int32_t offset_hz,
    uint8_t tx_power) {
    furi_check(chain);
    furi_check(setting);

    const uint8_t* source_preset =
        subghz_setting_get_preset_data_by_name(setting, GDR_TX_CARRIER_PRESET);
    if(!source_preset) {
        FURI_LOG_E(TAG, "Carrier preset %s is unavailable", GDR_TX_CARRIER_PRESET);
        return false;
    }

    size_t preset_count = subghz_setting_get_preset_count(setting);
    size_t source_size = 0;
    for(size_t i = 0; i < preset_count; i++) {
        if(strcmp(subghz_setting_get_preset_name(setting, i), GDR_TX_CARRIER_PRESET) ==
           0) {
            source_size = subghz_setting_get_preset_data_size(setting, i);
            break;
        }
    }
    if(!source_size) {
        FURI_LOG_E(TAG, "Carrier preset %s has zero size", GDR_TX_CARRIER_PRESET);
        return false;
    }

    if(chain->preset_data) {
        free(chain->preset_data);
        chain->preset_data = NULL;
        chain->preset_data_size = 0;
    }

    chain->preset_data =
        gdr_tx_chain_copy_preset(source_preset, source_size, &chain->preset_data_size);
    if(!chain->preset_data) {
        FURI_LOG_E(TAG, "Failed to copy preset data");
        return false;
    }

    gdr_tx_chain_apply_tx_power(chain->preset_data, chain->preset_data_size, tx_power);
    furi_string_set(chain->preset_name, GDR_TX_CARRIER_PRESET);

    const int64_t tx_frequency_signed = (int64_t)rx_frequency + offset_hz;
    if(tx_frequency_signed <= 0 || tx_frequency_signed > UINT32_MAX) {
        FURI_LOG_E(
            TAG,
            "TX offset out of range (rx=%lu offset=%ld)",
            rx_frequency,
            (long)offset_hz);
        return false;
    }
    const uint32_t tx_frequency = (uint32_t)tx_frequency_signed;
    if(!subghz_devices_is_frequency_valid(chain->device, tx_frequency)) {
        FURI_LOG_E(
            TAG,
            "Invalid TX frequency %lu (rx=%lu offset=%ld)",
            tx_frequency,
            rx_frequency,
            (long)offset_hz);
        return false;
    }

    chain->frequency = tx_frequency;
    return true;
}

bool gdr_tx_chain_start_carrier(GDRTxChain* chain) {
    furi_check(chain);
    if(!chain->device || !chain->data_gpio || !chain->preset_data) {
        FURI_LOG_E(TAG, "start rejected (incomplete stack)");
        return false;
    }
    if(chain->state == GDRTxRxStateTx) {
        return true;
    }

    subghz_devices_reset(chain->device);
    subghz_devices_idle(chain->device);
    subghz_devices_load_preset(chain->device, FuriHalSubGhzPresetCustom, chain->preset_data);
    subghz_devices_set_frequency(chain->device, chain->frequency);
    furi_hal_gpio_init(chain->data_gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(chain->data_gpio, true);
    if(!subghz_devices_set_tx(chain->device)) {
        FURI_LOG_E(TAG, "Carrier TX rejected on %lu Hz", chain->frequency);
        furi_hal_gpio_write(chain->data_gpio, false);
        furi_hal_gpio_init(chain->data_gpio, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        subghz_devices_idle(chain->device);
        return false;
    }
    chain->state = GDRTxRxStateTx;
    FURI_LOG_I(TAG, "Carrier TX started on %lu Hz", chain->frequency);
    return true;
}

void gdr_tx_chain_stop(GDRTxChain* chain) {
    if(!chain) {
        return;
    }
    if(chain->state != GDRTxRxStateTx) {
        return;
    }

    if(chain->device) {
        subghz_devices_idle(chain->device);
    }
    if(chain->data_gpio) {
        furi_hal_gpio_write(chain->data_gpio, false);
        furi_hal_gpio_init(chain->data_gpio, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    }
    chain->state = GDRTxRxStateIDLE;
}

#endif // ENABLE_SHIELD_RX_SCENE
