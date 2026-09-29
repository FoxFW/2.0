#pragma once

#include "subghz_types.h"

#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/raw.h>
#include <lib/subghz/devices/devices.h>

typedef struct SubGhzTxRx SubGhzTxRx;

typedef void (*SubGhzTxRxNeedSaveCallback)(void* context);

typedef enum {
    SubGhzTxRxStartTxStateOk,
    SubGhzTxRxStartTxStateErrorOnlyRx,
    SubGhzTxRxStartTxStateErrorParserOthers,
} SubGhzTxRxStartTxState;

SubGhzTxRx* subghz_txrx_alloc(void);

void subghz_txrx_free(SubGhzTxRx* instance);

bool subghz_txrx_is_database_loaded(SubGhzTxRx* instance);

void subghz_txrx_set_preset(
    SubGhzTxRx* instance,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);

uint8_t* subghz_txrx_set_tx_power(uint8_t* preset_data, size_t preset_data_size, uint8_t tx_power);

const char* subghz_txrx_get_preset_name(SubGhzTxRx* instance, const char* preset);

SubGhzRadioPreset subghz_txrx_get_preset(SubGhzTxRx* instance);

void subghz_txrx_get_frequency_and_modulation(
    SubGhzTxRx* instance,
    FuriString* frequency,
    FuriString* modulation,
    bool long_name);

SubGhzTxRxStartTxState subghz_txrx_tx_start(SubGhzTxRx* instance, FlipperFormat* flipper_format);

void subghz_txrx_rx_start(SubGhzTxRx* instance);

void subghz_txrx_stop(SubGhzTxRx* instance);

void subghz_txrx_sleep(SubGhzTxRx* instance);

void subghz_txrx_hopper_update(SubGhzTxRx* instance, float stay_threshold);

SubGhzHopperState subghz_txrx_hopper_get_state(SubGhzTxRx* instance);

void subghz_txrx_hopper_set_state(SubGhzTxRx* instance, SubGhzHopperState state);

void subghz_txrx_hopper_unpause(SubGhzTxRx* instance);

void subghz_txrx_hopper_pause(SubGhzTxRx* instance);

void subghz_txrx_preset_hopper_update(SubGhzTxRx* instance, float stay_threshold);

SubGhzPresetHopperState subghz_txrx_preset_hopper_get_state(SubGhzTxRx* instance);

void subghz_txrx_preset_hopper_set_state(SubGhzTxRx* instance, SubGhzPresetHopperState state);

void subghz_txrx_preset_hopper_unpause(SubGhzTxRx* instance);

void subghz_txrx_preset_hopper_pause(SubGhzTxRx* instance);

void subghz_txrx_preset_hopper_reset_index(SubGhzTxRx* instance, size_t index);

void subghz_txrx_speaker_on(SubGhzTxRx* instance);

void subghz_txrx_speaker_off(SubGhzTxRx* instance);

void subghz_txrx_speaker_mute(SubGhzTxRx* instance);

void subghz_txrx_speaker_unmute(SubGhzTxRx* instance);

void subghz_txrx_speaker_set_state(SubGhzTxRx* instance, SubGhzSpeakerState state);

SubGhzSpeakerState subghz_txrx_speaker_get_state(SubGhzTxRx* instance);

bool subghz_txrx_load_decoder_by_name_protocol(SubGhzTxRx* instance, const char* name_protocol);

SubGhzProtocolDecoderBase* subghz_txrx_get_decoder(SubGhzTxRx* instance);

void subghz_txrx_set_need_save_callback(
    SubGhzTxRx* instance,
    SubGhzTxRxNeedSaveCallback callback,
    void* context);

FlipperFormat* subghz_txrx_get_fff_data(SubGhzTxRx* instance);

SubGhzSetting* subghz_txrx_get_setting(SubGhzTxRx* instance);

bool subghz_txrx_protocol_is_serializable(SubGhzTxRx* instance);

bool subghz_txrx_protocol_is_transmittable(SubGhzTxRx* instance, bool check_type);

void subghz_txrx_receiver_set_filter(SubGhzTxRx* instance, SubGhzProtocolFlag filter);

void subghz_txrx_receiver_apply_modulation_filter(SubGhzTxRx* instance);

void subghz_txrx_set_protocol_enabled_callback(
    SubGhzTxRx* instance,
    SubGhzReceiverProtocolEnabledCallback callback,
    void* context);

void subghz_txrx_set_rx_callback(
    SubGhzTxRx* instance,
    SubGhzReceiverCallback callback,
    void* context);

void subghz_txrx_set_raw_file_encoder_worker_callback_end(
    SubGhzTxRx* instance,
    SubGhzProtocolEncoderRAWCallbackEnd callback,
    void* context);

bool subghz_txrx_radio_device_is_external_connected(SubGhzTxRx* instance, const char* name);

SubGhzRadioDeviceType
    subghz_txrx_radio_device_set(SubGhzTxRx* instance, SubGhzRadioDeviceType radio_device_type);

SubGhzRadioDeviceType subghz_txrx_radio_device_get(SubGhzTxRx* instance);

float subghz_txrx_radio_device_get_rssi(SubGhzTxRx* instance);

const char* subghz_txrx_radio_device_get_name(SubGhzTxRx* instance);

bool subghz_txrx_radio_device_is_frequency_valid(SubGhzTxRx* instance, uint32_t frequency);

bool subghz_txrx_radio_device_is_tx_allowed(SubGhzTxRx* instance, uint32_t frequency);

void subghz_txrx_set_debug_pin_state(SubGhzTxRx* instance, bool state);
bool subghz_txrx_get_debug_pin_state(SubGhzTxRx* instance);

void subghz_txrx_set_frequency_offset(SubGhzTxRx* instance, int32_t offset_hz);
int32_t subghz_txrx_get_frequency_offset(SubGhzTxRx* instance);

void subghz_txrx_reset_dynamic_and_custom_btns(SubGhzTxRx* instance);

SubGhzReceiver* subghz_txrx_get_receiver(SubGhzTxRx* instance);

void subghz_txrx_set_default_preset(SubGhzTxRx* instance, uint32_t frequency);

const char* subghz_txrx_set_preset_internal(
    SubGhzTxRx* instance,
    uint32_t frequency,
    uint8_t index,
    uint8_t tx_power);
