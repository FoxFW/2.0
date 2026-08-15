#pragma once

#if __has_include(<gpio_remap/gpio_remap_settings.h>)
#include <gpio_remap/gpio_remap_settings.h>
#else

/* Fallback if the shared gpio_remap service isn't available - private,
 * per-app copy of the same setting. */

#include <furi.h>
#include <saved_struct.h>
#include <storage/storage.h>

typedef enum {
    GpioRemapEsp32UartUsart = 0,
    GpioRemapEsp32UartLpuart = 1,
} GpioRemapEsp32Uart;

typedef struct {
    uint8_t esp32_uart_channel;
} GpioRemapSettings;

#define FOXDEFLOCK_GPIO_REMAP_FILE_NAME ".foxdeflock_gpio_remap.settings"
#define FOXDEFLOCK_GPIO_REMAP_PATH INT_PATH(FOXDEFLOCK_GPIO_REMAP_FILE_NAME)
#define FOXDEFLOCK_GPIO_REMAP_VER  (1)
#define FOXDEFLOCK_GPIO_REMAP_MAGIC (0x1A)

static inline void gpio_remap_settings_save(const GpioRemapSettings* settings) {
    furi_assert(settings);
    saved_struct_save(
        FOXDEFLOCK_GPIO_REMAP_PATH,
        settings,
        sizeof(GpioRemapSettings),
        FOXDEFLOCK_GPIO_REMAP_MAGIC,
        FOXDEFLOCK_GPIO_REMAP_VER);
}

static inline void gpio_remap_settings_load(GpioRemapSettings* settings) {
    furi_assert(settings);
    bool success = saved_struct_load(
        FOXDEFLOCK_GPIO_REMAP_PATH,
        settings,
        sizeof(GpioRemapSettings),
        FOXDEFLOCK_GPIO_REMAP_MAGIC,
        FOXDEFLOCK_GPIO_REMAP_VER);

    if(!success) {
        settings->esp32_uart_channel = GpioRemapEsp32UartUsart;
        gpio_remap_settings_save(settings);
    }
}

#endif
