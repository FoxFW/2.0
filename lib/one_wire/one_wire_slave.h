#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OneWireDevice OneWireDevice;
typedef struct OneWireSlave OneWireSlave;

typedef bool (*OneWireSlaveResetCallback)(bool is_short, void* context);
typedef bool (*OneWireSlaveCommandCallback)(uint8_t command, void* context);
typedef void (*OneWireSlaveResultCallback)(void* context);

OneWireSlave* onewire_slave_alloc(const GpioPin* gpio_pin);

void onewire_slave_free(OneWireSlave* bus);

void onewire_slave_start(OneWireSlave* bus);

void onewire_slave_stop(OneWireSlave* bus);

bool onewire_slave_receive_bit(OneWireSlave* bus);

bool onewire_slave_send_bit(OneWireSlave* bus, bool value);

bool onewire_slave_send(OneWireSlave* bus, const uint8_t* data, size_t data_size);

bool onewire_slave_receive(OneWireSlave* bus, uint8_t* data, size_t data_size);

void onewire_slave_set_overdrive(OneWireSlave* bus, bool set);

void onewire_slave_set_reset_callback(
    OneWireSlave* bus,
    OneWireSlaveResetCallback callback,
    void* context);

void onewire_slave_set_command_callback(
    OneWireSlave* bus,
    OneWireSlaveCommandCallback callback,
    void* context);

void onewire_slave_set_result_callback(
    OneWireSlave* bus,
    OneWireSlaveResultCallback result_cb,
    void* context);

#ifdef __cplusplus
}
#endif
