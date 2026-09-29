#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PULSE_READER_NO_EDGE   (0xFFFFFFFFUL)
#define PULSE_READER_LOST_EDGE (0xFFFFFFFEUL)
#define F_TIM2                 (64000000UL)

typedef enum {
    PulseReaderUnit64MHz,
    PulseReaderUnitPicosecond,
    PulseReaderUnitNanosecond,
    PulseReaderUnitMicrosecond,
} PulseReaderUnit;

typedef struct PulseReader PulseReader;

PulseReader* pulse_reader_alloc(const GpioPin* gpio, uint32_t size);

void pulse_reader_free(PulseReader* signal);

void pulse_reader_start(PulseReader* signal);

void pulse_reader_stop(PulseReader* signal);

uint32_t pulse_reader_receive(PulseReader* signal, int timeout_us);

uint32_t pulse_reader_samples(PulseReader* signal);

void pulse_reader_set_timebase(PulseReader* signal, PulseReaderUnit unit);

void pulse_reader_set_bittime(PulseReader* signal, uint32_t bit_time);

void pulse_reader_set_pull(PulseReader* signal, GpioPull pull);

#ifdef __cplusplus
}
#endif
