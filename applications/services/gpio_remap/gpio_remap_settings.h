#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Global ESP32 UART channel choice, shared by every Fox app that talks to
 * the ESP32 companion over UART (commander, detector, terminal, flasher,
 * uart_terminal) plus the "GPIO Pins" page in Fox Settings. Each consumer
 * loads fresh before use and saves on change - there is no live pubsub, so
 * a change only takes effect the next time another app (re)opens its own
 * connection settings, matching the existing cli_settings.c convention. */
typedef enum {
    GpioRemapEsp32UartUsart = 0, /* pins 13/14 */
    GpioRemapEsp32UartLpuart = 1, /* pins 15/16 */
} GpioRemapEsp32Uart;

typedef struct {
    uint8_t esp32_uart_channel; /* GpioRemapEsp32Uart */
} GpioRemapSettings;

void gpio_remap_settings_load(GpioRemapSettings* settings);
void gpio_remap_settings_save(const GpioRemapSettings* settings);

#ifdef __cplusplus
}
#endif
