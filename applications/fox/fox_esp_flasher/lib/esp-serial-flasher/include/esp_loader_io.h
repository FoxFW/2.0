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
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SERIAL_FLASHER_INTERFACE_SDIO

esp_loader_error_t loader_port_change_transmission_rate(uint32_t transmission_rate);
#endif

#ifndef SERIAL_FLASHER_INTERFACE_SDIO
esp_loader_error_t loader_port_write(const uint8_t *data, uint16_t size, uint32_t timeout);
#else
esp_loader_error_t loader_port_write(uint32_t function, uint32_t addr, const uint8_t *data,
                                     uint16_t size, uint32_t timeout);
#endif

#ifndef SERIAL_FLASHER_INTERFACE_SDIO
esp_loader_error_t loader_port_read(uint8_t *data, uint16_t size, uint32_t timeout);
#else
esp_loader_error_t loader_port_read(uint32_t function, uint32_t addr, uint8_t *data,
                                    uint16_t size, uint32_t timeout);
#endif

void loader_port_delay_ms(uint32_t ms);

void loader_port_start_timer(uint32_t ms);

uint32_t loader_port_remaining_time(void);

void loader_port_enter_bootloader(void);

void loader_port_reset_target(void);

void loader_port_debug_print(const char *str);

#ifdef SERIAL_FLASHER_INTERFACE_SPI

void loader_port_spi_set_cs(uint32_t level);
#endif

#ifdef SERIAL_FLASHER_INTERFACE_SDIO
esp_loader_error_t loader_port_sdio_card_init(void);
#endif

#ifdef __cplusplus
}
#endif
