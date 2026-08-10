#pragma once

#if __has_include(<gpio_remap/gpio_remap_settings.h>)
#include <gpio_remap/gpio_remap_settings.h>
#else

/* FoxFW2.0's shared gpio_remap service isn't available on this firmware -
 * fall back to a private, per-app copy of the same setting so the pin
 * choice still persists locally. Only the cross-app sync FoxFW2.0 gets
 * from the shared service is lost here. */

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

#define FOX_PORTAL_GPIO_REMAP_FILE_NAME ".fox_portal_gpio_remap.settings"
#define FOX_PORTAL_GPIO_REMAP_PATH INT_PATH(FOX_PORTAL_GPIO_REMAP_FILE_NAME)
#define FOX_PORTAL_GPIO_REMAP_VER  (1)
#define FOX_PORTAL_GPIO_REMAP_MAGIC (0x1A)

static inline void gpio_remap_settings_save(const GpioRemapSettings* settings) {
    furi_assert(settings);
    saved_struct_save(
        FOX_PORTAL_GPIO_REMAP_PATH,
        settings,
        sizeof(GpioRemapSettings),
        FOX_PORTAL_GPIO_REMAP_MAGIC,
        FOX_PORTAL_GPIO_REMAP_VER);
}

static inline void gpio_remap_settings_load(GpioRemapSettings* settings) {
    furi_assert(settings);
    bool success = saved_struct_load(
        FOX_PORTAL_GPIO_REMAP_PATH,
        settings,
        sizeof(GpioRemapSettings),
        FOX_PORTAL_GPIO_REMAP_MAGIC,
        FOX_PORTAL_GPIO_REMAP_VER);

    if(!success) {
        settings->esp32_uart_channel = GpioRemapEsp32UartUsart;
        gpio_remap_settings_save(settings);
    }
}

#endif
