#pragma once
#include <lib/subghz/devices/preset.h>
#include <lib/subghz/devices/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <toolbox/level_duration.h>
#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

void subghz_device_cc1101_ext_set_async_mirror_pin(const GpioPin* pin);

const GpioPin* subghz_device_cc1101_ext_get_data_gpio(void);

bool subghz_device_cc1101_ext_alloc(SubGhzDeviceConf* conf);

void subghz_device_cc1101_ext_free(void);

bool subghz_device_cc1101_ext_is_connect(void);

void subghz_device_cc1101_ext_sleep(void);

void subghz_device_cc1101_ext_dump_state(void);

void subghz_device_cc1101_ext_load_custom_preset(const uint8_t* preset_data);

void subghz_device_cc1101_ext_load_registers(const uint8_t* data);

void subghz_device_cc1101_ext_load_patable(const uint8_t data[8]);

void subghz_device_cc1101_ext_write_packet(const uint8_t* data, uint8_t size);

bool subghz_device_cc1101_ext_rx_pipe_not_empty(void);

bool subghz_device_cc1101_ext_is_rx_data_crc_valid(void);

void subghz_device_cc1101_ext_read_packet(uint8_t* data, uint8_t* size);

void subghz_device_cc1101_ext_flush_rx(void);

void subghz_device_cc1101_ext_flush_tx(void);

void subghz_device_cc1101_ext_shutdown(void);

void subghz_device_cc1101_ext_reset(void);

void subghz_device_cc1101_ext_idle(void);

void subghz_device_cc1101_ext_rx(void);

bool subghz_device_cc1101_ext_tx(void);

float subghz_device_cc1101_ext_get_rssi(void);

uint8_t subghz_device_cc1101_ext_get_lqi(void);

bool subghz_device_cc1101_ext_is_frequency_valid(uint32_t value);

uint32_t subghz_device_cc1101_ext_set_frequency(uint32_t value);

typedef void (*SubGhzDeviceCC1101ExtCaptureCallback)(bool level, uint32_t duration, void* context);

void subghz_device_cc1101_ext_start_async_rx(
    SubGhzDeviceCC1101ExtCaptureCallback callback,
    void* context);

void subghz_device_cc1101_ext_stop_async_rx(void);

typedef LevelDuration (*SubGhzDeviceCC1101ExtCallback)(void* context);

bool subghz_device_cc1101_ext_start_async_tx(SubGhzDeviceCC1101ExtCallback callback, void* context);

bool subghz_device_cc1101_ext_is_async_tx_complete(void);

void subghz_device_cc1101_ext_stop_async_tx(void);

#ifdef __cplusplus
}
#endif
