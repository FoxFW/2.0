#pragma once

#include <stddef.h>
#include <stdint.h>

#include "furi_hal_serial_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_serial_init(FuriHalSerialHandle* handle, uint32_t baud);

void furi_hal_serial_deinit(FuriHalSerialHandle* handle);

void furi_hal_serial_suspend(FuriHalSerialHandle* handle);

void furi_hal_serial_resume(FuriHalSerialHandle* handle);

bool furi_hal_serial_is_baud_rate_supported(FuriHalSerialHandle* handle, uint32_t baud);

void furi_hal_serial_set_br(FuriHalSerialHandle* handle, uint32_t baud);

void furi_hal_serial_configure_framing(
    FuriHalSerialHandle* handle,
    FuriHalSerialDataBits data_bits,
    FuriHalSerialParity parity,
    FuriHalSerialStopBits stop_bits);

void furi_hal_serial_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size);

void furi_hal_serial_tx_wait_complete(FuriHalSerialHandle* handle);

typedef enum {
    FuriHalSerialRxEventData = (1 << 0),
    FuriHalSerialRxEventIdle = (1 << 1),
    FuriHalSerialRxEventFrameError = (1 << 2),
    FuriHalSerialRxEventNoiseError = (1 << 3),
    FuriHalSerialRxEventOverrunError = (1 << 4),
    FuriHalSerialRxEventParityError = (1 << 5),
} FuriHalSerialRxEvent;

typedef void (*FuriHalSerialAsyncRxCallback)(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context);

void furi_hal_serial_async_rx_start(
    FuriHalSerialHandle* handle,
    FuriHalSerialAsyncRxCallback callback,
    void* context,
    bool report_errors);

void furi_hal_serial_async_rx_stop(FuriHalSerialHandle* handle);

bool furi_hal_serial_async_rx_available(FuriHalSerialHandle* handle);

uint8_t furi_hal_serial_async_rx(FuriHalSerialHandle* handle);

#define FURI_HAL_SERIAL_DMA_BUFFER_SIZE (256u)

typedef void (*FuriHalSerialDmaRxCallback)(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    size_t data_len,
    void* context);

void furi_hal_serial_enable_direction(
    FuriHalSerialHandle* handle,
    FuriHalSerialDirection direction);

void furi_hal_serial_disable_direction(
    FuriHalSerialHandle* handle,
    FuriHalSerialDirection direction);

const GpioPin*
    furi_hal_serial_get_gpio_pin(FuriHalSerialHandle* handle, FuriHalSerialDirection direction);

void furi_hal_serial_dma_rx_start(
    FuriHalSerialHandle* handle,
    FuriHalSerialDmaRxCallback callback,
    void* context,
    bool report_errors);

void furi_hal_serial_dma_rx_stop(FuriHalSerialHandle* handle);

size_t furi_hal_serial_dma_rx(FuriHalSerialHandle* handle, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
