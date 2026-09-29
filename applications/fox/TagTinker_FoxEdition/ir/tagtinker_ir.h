#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void tagtinker_ir_init(void);

void tagtinker_ir_deinit(void);

bool tagtinker_ir_transmit(const uint8_t* data, size_t len, uint16_t repeats, uint8_t delay);

bool tagtinker_ir_is_busy(void);

void tagtinker_ir_stop(void);
