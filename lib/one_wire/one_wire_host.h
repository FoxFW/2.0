#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OneWireHostSearchModeConditional = 0,
    OneWireHostSearchModeNormal = 1,
} OneWireHostSearchMode;

typedef struct OneWireHost OneWireHost;

OneWireHost* onewire_host_alloc(const GpioPin* gpio_pin);

void onewire_host_free(OneWireHost* host);

bool onewire_host_reset(OneWireHost* host);

bool onewire_host_read_bit(OneWireHost* host);

uint8_t onewire_host_read(OneWireHost* host);

void onewire_host_read_bytes(OneWireHost* host, uint8_t* buffer, uint16_t count);

void onewire_host_write_bit(OneWireHost* host, bool value);

void onewire_host_write(OneWireHost* host, uint8_t value);

void onewire_host_write_bytes(OneWireHost* host, const uint8_t* buffer, uint16_t count);

void onewire_host_start(OneWireHost* host);

void onewire_host_stop(OneWireHost* host);

void onewire_host_reset_search(OneWireHost* host);

void onewire_host_target_search(OneWireHost* host, uint8_t family_code);

bool onewire_host_search(OneWireHost* host, uint8_t* new_addr, OneWireHostSearchMode mode);

void onewire_host_set_overdrive(OneWireHost* host, bool set);

void onewire_host_set_timings_default(OneWireHost* host);

void onewire_host_set_timings_tm01x(OneWireHost* host);

#ifdef __cplusplus
}
#endif
