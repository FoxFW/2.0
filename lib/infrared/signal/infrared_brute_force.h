#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "infrared_error_code.h"
#include "infrared_signal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct InfraredBruteForce InfraredBruteForce;

InfraredBruteForce* infrared_brute_force_alloc(void);

void infrared_brute_force_free(InfraredBruteForce* brute_force);

void infrared_brute_force_set_db_filename(InfraredBruteForce* brute_force, const char* db_filename);

InfraredErrorCode infrared_brute_force_calculate_messages(InfraredBruteForce* brute_force);

bool infrared_brute_force_start(
    InfraredBruteForce* brute_force,
    uint32_t index,
    uint32_t* record_count);

bool infrared_brute_force_is_started(const InfraredBruteForce* brute_force);

void infrared_brute_force_stop(InfraredBruteForce* brute_force);

bool infrared_brute_force_send(InfraredBruteForce* brute_force, uint32_t signal_index);

bool infrared_brute_force_load_signal(
    InfraredBruteForce* brute_force,
    uint32_t signal_index,
    InfraredSignal* signal);

const char* infrared_brute_force_get_current_record_name(const InfraredBruteForce* brute_force);

void infrared_brute_force_add_record(
    InfraredBruteForce* brute_force,
    uint32_t index,
    const char* name);

void infrared_brute_force_reset(InfraredBruteForce* brute_force);

#ifdef __cplusplus
}
#endif
