#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*FuriHalIbuttonEmulateCallback)(void* context);

void furi_hal_ibutton_init(void);

void furi_hal_ibutton_emulate_start(
    uint32_t period,
    FuriHalIbuttonEmulateCallback callback,
    void* context);

void furi_hal_ibutton_emulate_set_next(uint32_t period);

void furi_hal_ibutton_emulate_stop(void);

void furi_hal_ibutton_pin_configure(void);

void furi_hal_ibutton_pin_reset(void);

void furi_hal_ibutton_pin_write(const bool state);

#ifdef __cplusplus
}
#endif
