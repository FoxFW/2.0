#pragma once

#include "cc1101_regs.h"

#include <stdbool.h>
#include <stdint.h>
#include <furi_hal_spi.h>

#ifdef __cplusplus
extern "C" {
#endif

CC1101Status cc1101_strobe(const FuriHalSpiBusHandle* handle, uint8_t strobe);

CC1101Status cc1101_write_reg(const FuriHalSpiBusHandle* handle, uint8_t reg, uint8_t data);

CC1101Status cc1101_read_reg(const FuriHalSpiBusHandle* handle, uint8_t reg, uint8_t* data);

CC1101Status cc1101_reset(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_get_status(const FuriHalSpiBusHandle* handle);

bool cc1101_wait_status_state(
    const FuriHalSpiBusHandle* handle,
    CC1101State state,
    uint32_t timeout_us);

CC1101Status cc1101_shutdown(const FuriHalSpiBusHandle* handle);

uint8_t cc1101_get_partnumber(const FuriHalSpiBusHandle* handle);

uint8_t cc1101_get_version(const FuriHalSpiBusHandle* handle);

uint8_t cc1101_get_rssi(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_calibrate(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_switch_to_idle(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_switch_to_rx(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_switch_to_tx(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_flush_rx(const FuriHalSpiBusHandle* handle);

CC1101Status cc1101_flush_tx(const FuriHalSpiBusHandle* handle);

uint32_t cc1101_set_frequency(const FuriHalSpiBusHandle* handle, uint32_t value);

uint32_t cc1101_set_intermediate_frequency(const FuriHalSpiBusHandle* handle, uint32_t value);

void cc1101_set_pa_table(const FuriHalSpiBusHandle* handle, const uint8_t value[8]);

uint8_t cc1101_write_fifo(const FuriHalSpiBusHandle* handle, const uint8_t* data, uint8_t size);

uint8_t cc1101_read_fifo(const FuriHalSpiBusHandle* handle, uint8_t* data, uint8_t* size);

#ifdef __cplusplus
}
#endif
