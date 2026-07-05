#pragma once

#include <furi.h>
#include <furi_hal.h>

typedef struct PredatorUart PredatorUart;

typedef void (*PredatorUartRxCallback)(uint8_t* buf, size_t len, void* context);

PredatorUart* predator_uart_init(const GpioPin* tx_pin, const GpioPin* rx_pin, uint32_t baud_rate, PredatorUartRxCallback callback, void* context);
void predator_uart_set_rx_callback(PredatorUart* uart, PredatorUartRxCallback callback, void* context);

void predator_uart_deinit(PredatorUart* uart);
void predator_uart_tx(PredatorUart* uart, uint8_t* data, size_t len);
void predator_uart_set_br(PredatorUart* uart, uint32_t baud);
