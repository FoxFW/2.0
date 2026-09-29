#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GpioRemapEsp32UartUsart = 0,
    GpioRemapEsp32UartLpuart = 1,
} GpioRemapEsp32Uart;

typedef struct {
    uint8_t esp32_uart_channel;
} GpioRemapSettings;

void gpio_remap_settings_load(GpioRemapSettings* settings);
void gpio_remap_settings_save(const GpioRemapSettings* settings);

#ifdef __cplusplus
}
#endif
