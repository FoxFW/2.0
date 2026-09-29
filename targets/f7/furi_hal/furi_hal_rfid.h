#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_rfid_init(void);

void furi_hal_rfid_pins_reset(void);

void furi_hal_rfid_pin_pull_release(void);

void furi_hal_rfid_pin_pull_pulldown(void);

void furi_hal_rfid_tim_read_start(float freq, float duty_cycle);

void furi_hal_rfid_tim_read_pause(void);

void furi_hal_rfid_tim_read_continue(void);

void furi_hal_rfid_tim_read_stop(void);

typedef void (*FuriHalRfidReadCaptureCallback)(bool level, uint32_t duration, void* context);

void furi_hal_rfid_tim_read_capture_start(FuriHalRfidReadCaptureCallback callback, void* context);

void furi_hal_rfid_tim_read_capture_stop(void);

typedef void (*FuriHalRfidDMACallback)(bool half, void* context);

void furi_hal_rfid_tim_emulate_dma_start(
    uint32_t* duration,
    uint32_t* pulse,
    size_t length,
    FuriHalRfidDMACallback callback,
    void* context);

void furi_hal_rfid_tim_emulate_dma_stop(void);

void furi_hal_rfid_set_read_period(uint32_t period);

void furi_hal_rfid_set_read_pulse(uint32_t pulse);

void furi_hal_rfid_comp_start(void);

void furi_hal_rfid_comp_stop(void);

typedef void (*FuriHalRfidCompCallback)(bool level, void* context);

void furi_hal_rfid_comp_set_callback(FuriHalRfidCompCallback callback, void* context);

void furi_hal_rfid_field_detect_start(void);

void furi_hal_rfid_field_detect_stop(void);

bool furi_hal_rfid_field_is_present(uint32_t* frequency);

#ifdef __cplusplus
}
#endif
