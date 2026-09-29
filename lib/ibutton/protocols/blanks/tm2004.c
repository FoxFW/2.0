#include "tm2004.h"

#include <core/kernel.h>

#define TM2004_CMD_READ_STATUS    0xAA
#define TM2004_CMD_READ_MEMORY    0xF0
#define TM2004_CMD_WRITE_ROM      0x3C
#define TM2004_CMD_FINALIZATION   0x35
#define TM2004_ANSWER_READ_MEMORY 0xF5

bool tm2004_write(OneWireHost* host, const uint8_t* data, size_t data_size) {
    onewire_host_set_timings_default(host);

    onewire_host_reset(host);
    onewire_host_write(host, TM2004_CMD_WRITE_ROM);

    onewire_host_write(host, 0x00);
    onewire_host_write(host, 0x00);

    size_t i;
    for(i = 0; i < data_size; ++i) {
        uint8_t answer;

        onewire_host_write(host, data[i]);
        answer = onewire_host_read(host);

        furi_delay_us(600);
        onewire_host_write_bit(host, true);
        furi_delay_us(50000);

        answer = onewire_host_read(host);

        if(data[i] != answer) {
            break;
        }
    }

    return i == data_size;
}
