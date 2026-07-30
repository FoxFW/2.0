/* Copyright 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define esp_loader_change_baudrate esp_loader_change_transmission_rate

#define RETURN_ON_ERROR(x) do {         \
    esp_loader_error_t _err_ = (x);     \
    if (_err_ != ESP_LOADER_SUCCESS) {  \
        return _err_;                   \
    }                                   \
} while(0)

typedef enum {
    ESP_LOADER_SUCCESS,
    ESP_LOADER_ERROR_FAIL,
    ESP_LOADER_ERROR_TIMEOUT,
    ESP_LOADER_ERROR_IMAGE_SIZE,
    ESP_LOADER_ERROR_INVALID_MD5,
    ESP_LOADER_ERROR_INVALID_PARAM,
    ESP_LOADER_ERROR_INVALID_TARGET,
    ESP_LOADER_ERROR_UNSUPPORTED_CHIP,
    ESP_LOADER_ERROR_UNSUPPORTED_FUNC,
    ESP_LOADER_ERROR_INVALID_RESPONSE
} esp_loader_error_t;

typedef enum {
    ESP8266_CHIP = 0,
    ESP32_CHIP   = 1,
    ESP32S2_CHIP = 2,
    ESP32C3_CHIP = 3,
    ESP32S3_CHIP = 4,
    ESP32C2_CHIP = 5,
    ESP32C5_CHIP = 6,
    ESP32H2_CHIP = 7,
    ESP32C6_CHIP = 8,
    ESP32P4_CHIP = 9,
    ESP_MAX_CHIP = 10,
    ESP_UNKNOWN_CHIP = 10
} target_chip_t;

typedef struct {
    uint8_t magic;
    uint8_t segments;
    uint8_t flash_mode;
    uint8_t flash_size_freq;
    uint32_t entrypoint;
} esp_loader_bin_header_t;

typedef struct {
    uint32_t addr;
    uint32_t size;
    uint8_t *data;
} esp_loader_bin_segment_t;

typedef struct {
    target_chip_t target_chip;
    uint32_t eco_version;
    bool secure_boot_enabled;
    bool secure_boot_aggressive_revoke_enabled;
    bool secure_download_mode_enabled;
    bool secure_boot_revoked_keys[3];
    bool jtag_software_disabled;
    bool jtag_hardware_disabled;
    bool usb_disabled;
    bool flash_encryption_enabled;
    bool dcache_in_uart_download_disabled;
    bool icache_in_uart_download_disabled;
} esp_loader_target_security_info_t;

typedef struct {
    uint32_t sync_timeout;
    int32_t trials;
} esp_loader_connect_args_t;

#define ESP_LOADER_CONNECT_DEFAULT() { \
  .sync_timeout = 100, \
  .trials = 10, \
}

esp_loader_error_t esp_loader_connect(esp_loader_connect_args_t *connect_args);

target_chip_t esp_loader_get_target(void);

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)

esp_loader_error_t esp_loader_connect_with_stub(esp_loader_connect_args_t *connect_args);

#ifdef SERIAL_FLASHER_INTERFACE_UART

esp_loader_error_t esp_loader_connect_secure_download_mode(esp_loader_connect_args_t *connect_args,
        uint32_t flash_size, target_chip_t target_chip);
#endif
#endif

#ifndef SERIAL_FLASHER_INTERFACE_SPI

esp_loader_error_t esp_loader_flash_start(uint32_t offset, uint32_t image_size, uint32_t block_size);

esp_loader_error_t esp_loader_flash_write(void *payload, uint32_t size);

esp_loader_error_t esp_loader_flash_finish(bool reboot);

esp_loader_error_t esp_loader_flash_detect_size(uint32_t *flash_size);
#endif

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)

esp_loader_error_t esp_loader_flash_read(uint8_t *buf, uint32_t address, uint32_t length);

esp_loader_error_t esp_loader_flash_erase(void);

esp_loader_error_t esp_loader_flash_erase_region(uint32_t offset, uint32_t size);

esp_loader_error_t esp_loader_change_transmission_rate_stub(uint32_t old_transmission_rate,
        uint32_t new_transmission_rate);

esp_loader_error_t esp_loader_get_security_info(esp_loader_target_security_info_t *security_info);
#endif

esp_loader_error_t esp_loader_mem_start(uint32_t offset, uint32_t size, uint32_t block_size);

esp_loader_error_t esp_loader_mem_write(const void *payload, uint32_t size);

esp_loader_error_t esp_loader_mem_finish(uint32_t entrypoint);

esp_loader_error_t esp_loader_read_mac(uint8_t *mac);

esp_loader_error_t esp_loader_write_register(uint32_t address, uint32_t reg_value);

esp_loader_error_t esp_loader_read_register(uint32_t address, uint32_t *reg_value);

#ifndef SERIAL_FLASHER_INTERFACE_SDIO

esp_loader_error_t esp_loader_change_transmission_rate(uint32_t transmission_rate);
#endif

#if MD5_ENABLED

esp_loader_error_t esp_loader_flash_verify_known_md5(uint32_t address,
        uint32_t size,
        const uint8_t *expected_md5);

esp_loader_error_t esp_loader_flash_verify(void);
#endif

void esp_loader_reset_target(void);

#ifdef __cplusplus
}
#endif
