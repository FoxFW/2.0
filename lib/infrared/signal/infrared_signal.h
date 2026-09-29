#pragma once

#include "infrared_error_code.h"
#include <flipper_format/flipper_format.h>
#include <infrared.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct InfraredSignal InfraredSignal;

typedef struct {
    size_t timings_size;
    uint32_t* timings;
    uint32_t frequency;
    float duty_cycle;
} InfraredRawSignal;

InfraredSignal* infrared_signal_alloc(void);

void infrared_signal_free(InfraredSignal* signal);

bool infrared_signal_is_raw(const InfraredSignal* signal);

bool infrared_signal_is_valid(const InfraredSignal* signal);

void infrared_signal_set_signal(InfraredSignal* signal, const InfraredSignal* other);

void infrared_signal_set_raw_signal(
    InfraredSignal* signal,
    const uint32_t* timings,
    size_t timings_size,
    uint32_t frequency,
    float duty_cycle);

const InfraredRawSignal* infrared_signal_get_raw_signal(const InfraredSignal* signal);

void infrared_signal_set_message(InfraredSignal* signal, const InfraredMessage* message);

const InfraredMessage* infrared_signal_get_message(const InfraredSignal* signal);

InfraredErrorCode
    infrared_signal_read(InfraredSignal* signal, FlipperFormat* ff, FuriString* name);

InfraredErrorCode infrared_signal_read_name(FlipperFormat* ff, FuriString* name);

InfraredErrorCode infrared_signal_read_body(InfraredSignal* signal, FlipperFormat* ff);

InfraredErrorCode infrared_signal_search_by_name_and_read(
    InfraredSignal* signal,
    FlipperFormat* ff,
    const char* name);

InfraredErrorCode infrared_signal_search_by_index_and_read(
    InfraredSignal* signal,
    FlipperFormat* ff,
    size_t index);

InfraredErrorCode
    infrared_signal_save(const InfraredSignal* signal, FlipperFormat* ff, const char* name);

void infrared_signal_transmit(const InfraredSignal* signal);

#ifdef __cplusplus
}
#endif
