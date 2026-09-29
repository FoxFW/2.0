#include <furi.h>

#include "one_wire_host.h"

typedef struct {
    uint16_t a;
    uint16_t b;
    uint16_t c;
    uint16_t d;
    uint16_t e;
    uint16_t f;
    uint16_t g;
    uint16_t h;
    uint16_t i;
    uint16_t j;
} OneWireHostTimings;

static const OneWireHostTimings onewire_host_timings_normal = {
    .a = 9,
    .b = 64,
    .c = 64,
    .d = 14,
    .e = 9,
    .f = 55,
    .g = 0,
    .h = 480,
    .i = 70,
    .j = 410,
};

static const OneWireHostTimings onewire_host_timings_overdrive = {
    .a = 1,
    .b = 8,
    .c = 8,
    .d = 3,
    .e = 1,
    .f = 7,
    .g = 3,
    .h = 70,
    .i = 9,
    .j = 40,
};

static const OneWireHostTimings onewire_host_timings_tm01x = {
    .a = 5,
    .b = 80,
    .c = 70,
    .d = 10,
    .e = 5,
    .f = 70,
    .g = 0,
    .h = 740,
    .i = 140,
    .j = 410,
};

struct OneWireHost {
    const GpioPin* gpio_pin;
    const OneWireHostTimings* timings;
    unsigned char saved_rom[8];
    uint8_t last_discrepancy;
    uint8_t last_family_discrepancy;
    bool last_device_flag;
};

OneWireHost* onewire_host_alloc(const GpioPin* gpio_pin) {
    furi_check(gpio_pin);

    OneWireHost* host = malloc(sizeof(OneWireHost));
    host->gpio_pin = gpio_pin;
    onewire_host_reset_search(host);
    onewire_host_set_overdrive(host, false);

    return host;
}

void onewire_host_free(OneWireHost* host) {
    furi_check(host);

    onewire_host_stop(host);
    free(host);
}

bool onewire_host_reset(OneWireHost* host) {
    furi_check(host);

    uint8_t r;
    uint8_t retries = 125;

    const OneWireHostTimings* timings = host->timings;

    furi_hal_gpio_write(host->gpio_pin, true);
    do {
        if(--retries == 0) return 0;
        furi_delay_us(2);
    } while(!furi_hal_gpio_read(host->gpio_pin));

    furi_delay_us(timings->g);

    furi_hal_gpio_write(host->gpio_pin, false);
    furi_delay_us(timings->h);

    furi_hal_gpio_write(host->gpio_pin, true);
    furi_delay_us(timings->i);

    r = !furi_hal_gpio_read(host->gpio_pin);
    furi_delay_us(timings->j);

    return r;
}

bool onewire_host_read_bit(OneWireHost* host) {
    furi_check(host);

    bool result;

    const OneWireHostTimings* timings = host->timings;

    furi_hal_gpio_write(host->gpio_pin, false);
    furi_delay_us(timings->a);

    furi_hal_gpio_write(host->gpio_pin, true);
    furi_delay_us(timings->e);

    result = furi_hal_gpio_read(host->gpio_pin);
    furi_delay_us(timings->f);

    return result;
}

uint8_t onewire_host_read(OneWireHost* host) {
    furi_check(host);

    uint8_t result = 0;

    for(uint8_t bitMask = 0x01; bitMask; bitMask <<= 1) {
        if(onewire_host_read_bit(host)) {
            result |= bitMask;
        }
    }

    return result;
}

void onewire_host_read_bytes(OneWireHost* host, uint8_t* buffer, uint16_t count) {
    furi_check(host);
    furi_check(buffer);

    for(uint16_t i = 0; i < count; i++) {
        buffer[i] = onewire_host_read(host);
    }
}

void onewire_host_write_bit(OneWireHost* host, bool value) {
    furi_check(host);

    const OneWireHostTimings* timings = host->timings;

    if(value) {

        furi_hal_gpio_write(host->gpio_pin, false);
        furi_delay_us(timings->a);

        furi_hal_gpio_write(host->gpio_pin, true);
        furi_delay_us(timings->b);
    } else {

        furi_hal_gpio_write(host->gpio_pin, false);
        furi_delay_us(timings->c);

        furi_hal_gpio_write(host->gpio_pin, true);
        furi_delay_us(timings->d);
    }
}

void onewire_host_write(OneWireHost* host, uint8_t value) {
    furi_check(host);

    uint8_t bitMask;

    for(bitMask = 0x01; bitMask; bitMask <<= 1) {
        onewire_host_write_bit(host, (bitMask & value) ? 1 : 0);
    }
}

void onewire_host_write_bytes(OneWireHost* host, const uint8_t* buffer, uint16_t count) {
    furi_check(host);
    furi_check(buffer);

    for(uint16_t i = 0; i < count; ++i) {
        onewire_host_write(host, buffer[i]);
    }
}

void onewire_host_start(OneWireHost* host) {
    furi_check(host);

    furi_hal_gpio_write(host->gpio_pin, true);
    furi_hal_gpio_init(host->gpio_pin, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
}

void onewire_host_stop(OneWireHost* host) {
    furi_check(host);

    furi_hal_gpio_write(host->gpio_pin, true);
    furi_hal_gpio_init(host->gpio_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

void onewire_host_reset_search(OneWireHost* host) {
    furi_check(host);

    host->last_discrepancy = 0;
    host->last_device_flag = false;
    host->last_family_discrepancy = 0;
    for(int i = 7;; i--) {
        host->saved_rom[i] = 0;
        if(i == 0) break;
    }
}

void onewire_host_target_search(OneWireHost* host, uint8_t family_code) {
    furi_check(host);

    host->saved_rom[0] = family_code;
    for(uint8_t i = 1; i < 8; i++)
        host->saved_rom[i] = 0;
    host->last_discrepancy = 64;
    host->last_family_discrepancy = 0;
    host->last_device_flag = false;
}

bool onewire_host_search(OneWireHost* host, uint8_t* new_addr, OneWireHostSearchMode mode) {
    furi_check(host);

    uint8_t id_bit_number;
    uint8_t last_zero, rom_byte_number, search_result;
    uint8_t id_bit, cmp_id_bit;

    unsigned char rom_byte_mask, search_direction;

    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = 0;

    if(!host->last_device_flag) {

        if(!onewire_host_reset(host)) {

            host->last_discrepancy = 0;
            host->last_device_flag = false;
            host->last_family_discrepancy = 0;
            return false;
        }

        switch(mode) {
        case OneWireHostSearchModeConditional:
            onewire_host_write(host, 0xEC);
            break;
        case OneWireHostSearchModeNormal:
            onewire_host_write(host, 0xF0);
            break;
        }

        do {

            id_bit = onewire_host_read_bit(host);
            cmp_id_bit = onewire_host_read_bit(host);

            if((id_bit == 1) && (cmp_id_bit == 1))
                break;
            else {

                if(id_bit != cmp_id_bit)
                    search_direction = id_bit;
                else {

                    if(id_bit_number < host->last_discrepancy)
                        search_direction =
                            ((host->saved_rom[rom_byte_number] & rom_byte_mask) > 0);
                    else

                        search_direction = (id_bit_number == host->last_discrepancy);

                    if(search_direction == 0) {
                        last_zero = id_bit_number;

                        if(last_zero < 9) host->last_family_discrepancy = last_zero;
                    }
                }

                if(search_direction == 1)
                    host->saved_rom[rom_byte_number] |= rom_byte_mask;
                else
                    host->saved_rom[rom_byte_number] &= ~rom_byte_mask;

                onewire_host_write_bit(host, search_direction);

                id_bit_number++;
                rom_byte_mask <<= 1;

                if(rom_byte_mask == 0) {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while(rom_byte_number < 8);

        if(!(id_bit_number < 65)) {

            host->last_discrepancy = last_zero;

            if(host->last_discrepancy == 0) host->last_device_flag = true;

            search_result = true;
        }
    }

    if(!search_result || !host->saved_rom[0]) {
        host->last_discrepancy = 0;
        host->last_device_flag = false;
        host->last_family_discrepancy = 0;
        search_result = false;
    } else {
        for(int i = 0; i < 8; i++)
            new_addr[i] = host->saved_rom[i];
    }

    return search_result;
}

void onewire_host_set_overdrive(OneWireHost* host, bool set) {
    furi_check(host);

    host->timings = set ? &onewire_host_timings_overdrive : &onewire_host_timings_normal;
}

void onewire_host_set_timings_default(OneWireHost* host) {
    furi_check(host);

    host->timings = &onewire_host_timings_normal;
}

void onewire_host_set_timings_tm01x(OneWireHost* host) {
    furi_check(host);

    host->timings = &onewire_host_timings_tm01x;
}
