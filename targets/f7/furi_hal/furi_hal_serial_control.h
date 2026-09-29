#pragma once

#include "furi_hal_serial_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_serial_control_init(void);

void furi_hal_serial_control_deinit(void);

void furi_hal_serial_control_suspend(void);

void furi_hal_serial_control_resume(void);

FuriHalSerialHandle* furi_hal_serial_control_acquire(FuriHalSerialId serial_id);

void furi_hal_serial_control_release(FuriHalSerialHandle* handle);

bool furi_hal_serial_control_is_busy(FuriHalSerialId serial_id);

void furi_hal_serial_control_set_logging_config(FuriHalSerialId serial_id, uint32_t baud_rate);

typedef void (*FuriHalSerialControlExpansionCallback)(void* context);

void furi_hal_serial_control_set_expansion_callback(
    FuriHalSerialId serial_id,
    FuriHalSerialControlExpansionCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
