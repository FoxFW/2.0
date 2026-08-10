#include "gpio_remap_settings.h"

#include <furi.h>
#include <saved_struct.h>
#include <storage/storage.h>

#define GPIO_REMAP_SETTINGS_FILE_NAME ".gpio_remap.settings"
#define GPIO_REMAP_SETTINGS_PATH       INT_PATH(GPIO_REMAP_SETTINGS_FILE_NAME)
#define GPIO_REMAP_SETTINGS_VER        (1)
#define GPIO_REMAP_SETTINGS_MAGIC      (0x1A)

void gpio_remap_settings_load(GpioRemapSettings* settings) {
    furi_assert(settings);

    bool success = saved_struct_load(
        GPIO_REMAP_SETTINGS_PATH,
        settings,
        sizeof(GpioRemapSettings),
        GPIO_REMAP_SETTINGS_MAGIC,
        GPIO_REMAP_SETTINGS_VER);

    if(!success) {
        settings->esp32_uart_channel = GpioRemapEsp32UartUsart;
        gpio_remap_settings_save(settings);
    }
}

void gpio_remap_settings_save(const GpioRemapSettings* settings) {
    furi_assert(settings);

    saved_struct_save(
        GPIO_REMAP_SETTINGS_PATH,
        settings,
        sizeof(GpioRemapSettings),
        GPIO_REMAP_SETTINGS_MAGIC,
        GPIO_REMAP_SETTINGS_VER);
}

void gpio_remap_on_system_start(void) {
}
