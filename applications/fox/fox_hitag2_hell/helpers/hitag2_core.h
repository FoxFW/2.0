#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Hitag2State;

#define HITAG2_STATE_MASK ((uint64_t)0xFFFFFFFFFFFFULL)

uint8_t hitag2_fiat_filter(Hitag2State state);

uint8_t hitag2_fiat_lfsr_feedback(Hitag2State state);

static inline Hitag2State hitag2_shift(Hitag2State state, uint8_t input) {
    return ((state << 1U) | ((uint64_t)(input & 1U))) & HITAG2_STATE_MASK;
}

Hitag2State hitag2_fiat_init_phase(
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    const uint8_t key[6],
    uint32_t epoch);

uint32_t hitag2_fiat_full_auth(
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    const uint8_t key[6],
    uint32_t epoch);

bool hitag2_fiat_invert_init(
    Hitag2State state31,
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    uint32_t epoch,
    uint8_t key_out[6]);

bool hitag2_fiat_self_test(void);

#ifdef __cplusplus
}
#endif
