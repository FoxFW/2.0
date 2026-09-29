#include "hitag2_core.h"

#include <string.h>

#define HITAG2_FA_TABLE 0x2C79UL
#define HITAG2_FB_TABLE 0x6671UL
#define HITAG2_FC_TABLE 0x7907287BUL

static inline uint8_t hitag2_truth(uint32_t table, uint8_t index) {
    return (uint8_t)((table >> index) & 1U);
}

static inline uint8_t hitag2_bit(Hitag2State state, uint8_t n) {
    return (uint8_t)((state >> n) & 1U);
}

static inline uint8_t hitag2_fi(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint8_t)((a << 3U) | (b << 2U) | (c << 1U) | d);
}

uint8_t hitag2_fiat_filter(Hitag2State s) {

    const uint8_t g0 = hitag2_truth(
        HITAG2_FA_TABLE,
        hitag2_fi(hitag2_bit(s, 41U), hitag2_bit(s, 42U), hitag2_bit(s, 44U), hitag2_bit(s, 45U)));

    const uint8_t g1 = hitag2_truth(
        HITAG2_FB_TABLE,
        hitag2_fi(hitag2_bit(s, 32U), hitag2_bit(s, 33U), hitag2_bit(s, 35U), hitag2_bit(s, 39U)));

    const uint8_t g2 = hitag2_truth(
        HITAG2_FB_TABLE,
        hitag2_fi(hitag2_bit(s, 21U), hitag2_bit(s, 24U), hitag2_bit(s, 26U), hitag2_bit(s, 30U)));

    const uint8_t g3 = hitag2_truth(
        HITAG2_FB_TABLE,
        hitag2_fi(hitag2_bit(s, 14U), hitag2_bit(s, 16U), hitag2_bit(s, 18U), hitag2_bit(s, 19U)));

    const uint8_t g4 = hitag2_truth(
        HITAG2_FA_TABLE,
        hitag2_fi(hitag2_bit(s, 1U), hitag2_bit(s, 3U), hitag2_bit(s, 4U), hitag2_bit(s, 13U)));
    const uint8_t group = (uint8_t)(g0 | (g1 << 1U) | (g2 << 2U) | (g3 << 3U) | (g4 << 4U));
    return hitag2_truth(HITAG2_FC_TABLE, group);
}

uint8_t hitag2_fiat_lfsr_feedback(Hitag2State s) {

    static const uint64_t tap_mask =
        (1ULL << 0) | (1ULL << 1) | (1ULL << 4) | (1ULL << 5) | (1ULL << 6) | (1ULL << 17) |
        (1ULL << 21) | (1ULL << 24) | (1ULL << 25) | (1ULL << 31) | (1ULL << 39) |
        (1ULL << 40) | (1ULL << 41) | (1ULL << 44) | (1ULL << 45) | (1ULL << 47);
    uint64_t v = s & tap_mask;

    v ^= v >> 32;
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return (uint8_t)(v & 1U);
}

static inline uint8_t hitag2_iv_bit_be(uint32_t iv, uint8_t index) {
    return (uint8_t)((iv >> (31U - index)) & 1U);
}

static inline uint8_t hitag2_key_bit_be(const uint8_t* key, uint8_t index) {
    return (uint8_t)((key[index >> 3U] >> (7U - (index & 7U))) & 1U);
}

static inline uint32_t hitag2_build_iv(uint8_t button, uint16_t control, uint32_t epoch) {
    return ((epoch & 0x3FFFFUL) << 14U) | (((uint32_t)control & 0x3FFUL) << 4U) |
           ((uint32_t)button & 0xFUL);
}

static inline Hitag2State hitag2_build_initial_state(uint32_t uid, const uint8_t key[6]) {

    return (((uint64_t)((uid >> 24) & 0xFFU)) << 40) |
           (((uint64_t)((uid >> 16) & 0xFFU)) << 32) |
           (((uint64_t)((uid >>  8) & 0xFFU)) << 24) |
           (((uint64_t)((uid      ) & 0xFFU)) << 16) |
           (((uint64_t)(key[4])) << 8) | ((uint64_t)key[5]);
}

Hitag2State hitag2_fiat_init_phase(
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    const uint8_t key[6],
    uint32_t epoch) {
    Hitag2State s = hitag2_build_initial_state(uid, key);
    const uint32_t iv = hitag2_build_iv(button, control, epoch);
    for(uint8_t i = 0U; i < 32U; i++) {
        const uint8_t input =
            (uint8_t)(hitag2_iv_bit_be(iv, i) ^ hitag2_key_bit_be(key, i) ^ hitag2_fiat_filter(s));
        s = hitag2_shift(s, input);
    }
    return s;
}

uint32_t hitag2_fiat_full_auth(
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    const uint8_t key[6],
    uint32_t epoch) {
    Hitag2State s = hitag2_fiat_init_phase(uid, button, control, key, epoch);
    uint32_t authenticator = 0U;
    for(uint8_t i = 0U; i < 32U; i++) {
        authenticator = (authenticator << 1U) | hitag2_fiat_filter(s);
        s = hitag2_shift(s, hitag2_fiat_lfsr_feedback(s));
    }
    return authenticator;
}

bool hitag2_fiat_invert_init(
    Hitag2State state31,
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    uint32_t epoch,
    uint8_t key_out[6]) {
    const uint32_t iv = hitag2_build_iv(button, control, epoch);
    Hitag2State s = state31 & HITAG2_STATE_MASK;
    uint8_t key_bits[32];

    for(int i = 31; i >= 0; i--) {

        const uint8_t input_i = (uint8_t)(s & 1U);

        const uint8_t msb_prev = (uint8_t)((uid >> (31 - i)) & 1U);
        s = (s >> 1) | ((uint64_t)msb_prev << 47);
        s &= HITAG2_STATE_MASK;

        const uint8_t filter_i = hitag2_fiat_filter(s);
        const uint8_t iv_bit = hitag2_iv_bit_be(iv, (uint8_t)i);
        key_bits[i] = (uint8_t)(input_i ^ iv_bit ^ filter_i);
    }

    const uint32_t recovered_uid = (uint32_t)((s >> 16) & 0xFFFFFFFFULL);
    const bool consistent = (recovered_uid == uid);

    for(uint8_t b = 0U; b < 4U; b++) {
        uint8_t byte = 0U;
        for(uint8_t j = 0U; j < 8U; j++) {
            byte = (uint8_t)((byte << 1U) | key_bits[b * 8U + j]);
        }
        key_out[b] = byte;
    }
    key_out[4] = (uint8_t)((s >> 8) & 0xFFU);
    key_out[5] = (uint8_t)(s & 0xFFU);

    return consistent;
}

static uint8_t hitag2_ref_truth(uint32_t table, uint8_t index) {
    return (uint8_t)((table >> index) & 1U);
}

static uint8_t hitag2_ref_filter_index(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint8_t)((a << 3U) | (b << 2U) | (c << 1U) | d);
}

static uint8_t hitag2_ref_byte_bit(uint8_t byte, uint8_t bit) {
    return (uint8_t)((byte >> bit) & 1U);
}

static uint8_t hitag2_ref_filter(const uint8_t state[6]) {
    uint8_t group = 0U;
    group |= hitag2_ref_truth(
        0x2c79U,
        hitag2_ref_filter_index(
            hitag2_ref_byte_bit(state[0], 1U),
            hitag2_ref_byte_bit(state[0], 2U),
            hitag2_ref_byte_bit(state[0], 4U),
            hitag2_ref_byte_bit(state[0], 5U)));
    group |= (uint8_t)(hitag2_ref_truth(
                            0x6671U,
                            hitag2_ref_filter_index(
                                hitag2_ref_byte_bit(state[1], 0U),
                                hitag2_ref_byte_bit(state[1], 1U),
                                hitag2_ref_byte_bit(state[1], 3U),
                                hitag2_ref_byte_bit(state[1], 7U)))
                        << 1U);
    group |= (uint8_t)(hitag2_ref_truth(
                            0x6671U,
                            hitag2_ref_filter_index(
                                hitag2_ref_byte_bit(state[3], 5U),
                                hitag2_ref_byte_bit(state[2], 0U),
                                hitag2_ref_byte_bit(state[2], 2U),
                                hitag2_ref_byte_bit(state[2], 6U)))
                        << 2U);
    group |= (uint8_t)(hitag2_ref_truth(
                            0x6671U,
                            hitag2_ref_filter_index(
                                hitag2_ref_byte_bit(state[4], 6U),
                                hitag2_ref_byte_bit(state[3], 0U),
                                hitag2_ref_byte_bit(state[3], 2U),
                                hitag2_ref_byte_bit(state[3], 3U)))
                        << 3U);
    group |= (uint8_t)(hitag2_ref_truth(
                            0x2c79U,
                            hitag2_ref_filter_index(
                                hitag2_ref_byte_bit(state[5], 1U),
                                hitag2_ref_byte_bit(state[5], 3U),
                                hitag2_ref_byte_bit(state[5], 4U),
                                hitag2_ref_byte_bit(state[4], 5U)))
                        << 4U);
    return hitag2_ref_truth(0x7907287bUL, group);
}

static uint8_t hitag2_ref_parity8(uint8_t value) {
    value ^= (uint8_t)(value >> 4U);
    value ^= (uint8_t)(value >> 2U);
    value ^= (uint8_t)(value >> 1U);
    return value & 1U;
}

static uint8_t hitag2_ref_feedback(const uint8_t state[6]) {
    static const uint8_t masks[6] = {0xb3U, 0x80U, 0x83U, 0x22U, 0x00U, 0x73U};
    uint8_t feedback = 0U;
    for(uint8_t i = 0U; i < 6U; i++) {
        feedback ^= hitag2_ref_parity8((uint8_t)(state[i] & masks[i]));
    }
    return feedback & 1U;
}

static void hitag2_ref_shift(uint8_t state[6], uint8_t input) {
    for(uint8_t i = 0U; i < 5U; i++) {
        state[i] = (uint8_t)((state[i] << 1U) | (state[i + 1U] >> 7U));
    }
    state[5] = (uint8_t)((state[5] << 1U) | (input & 1U));
}

static uint8_t hitag2_ref_input_bit_u32_be(uint32_t value, uint8_t index) {
    return (uint8_t)((value >> (31U - index)) & 1U);
}

static uint8_t hitag2_ref_input_bit_bytes_be(const uint8_t* bytes, uint8_t index) {
    return (uint8_t)((bytes[index >> 3U] >> (7U - (index & 7U))) & 1U);
}

static uint32_t hitag2_self_test_reference_auth(
    uint32_t uid,
    uint8_t button,
    uint16_t control,
    const uint8_t key[6],
    uint32_t epoch) {
    uint8_t state[6] = {
        (uint8_t)(uid >> 24U),
        (uint8_t)(uid >> 16U),
        (uint8_t)(uid >> 8U),
        (uint8_t)uid,
        key[4],
        key[5],
    };

    const uint32_t iv = ((epoch & 0x3FFFFUL) << 14U) | (((uint32_t)control & 0x03FFUL) << 4U) |
                         ((uint32_t)button & 0x0FUL);

    for(uint8_t i = 0U; i < 32U; i++) {
        const uint8_t input = hitag2_ref_input_bit_u32_be(iv, i) ^
                               hitag2_ref_input_bit_bytes_be(key, i) ^ hitag2_ref_filter(state);
        hitag2_ref_shift(state, input);
    }

    uint32_t authenticator = 0U;
    for(uint8_t i = 0U; i < 32U; i++) {
        authenticator = (authenticator << 1U) | hitag2_ref_filter(state);
        hitag2_ref_shift(state, hitag2_ref_feedback(state));
    }
    return authenticator;
}

static const uint8_t k_hitag2_fiat_self_test_keys[8][6] = {
    {0xB7U, 0x92U, 0x80U, 0xAEU, 0xCCU, 0x37U},
    {0xD4U, 0x24U, 0x28U, 0xF7U, 0xD9U, 0x66U},
    {0x4DU, 0x34U, 0x3FU, 0xD4U, 0xE7U, 0xB6U},
    {0x6DU, 0x6BU, 0xF2U, 0x1DU, 0x3AU, 0x1AU},
    {0xA3U, 0xF3U, 0xACU, 0xF7U, 0xB9U, 0x10U},
    {0x4DU, 0x49U, 0x4BU, 0x52U, 0x4FU, 0x4EU},
    {0xCDU, 0x49U, 0x4BU, 0x52U, 0x4FU, 0x4EU},
    {0x33U, 0xFAU, 0x2FU, 0xCDU, 0xC3U, 0x3BU},
};

static uint32_t hitag2_self_test_rand(uint32_t* seed) {
    *seed = (*seed) * 1664525U + 1013904223U;
    return *seed;
}

bool hitag2_fiat_self_test(void) {
    const uint8_t trials_per_key = 16U;
    uint32_t seed = 0xC0FFEE01U;

    for(uint8_t k = 0U; k < 8U; k++) {
        const uint8_t* key = k_hitag2_fiat_self_test_keys[k];
        for(uint8_t t = 0U; t < trials_per_key; t++) {
            const uint32_t uid = hitag2_self_test_rand(&seed);
            const uint32_t r1 = hitag2_self_test_rand(&seed);
            const uint8_t button = (uint8_t)(r1 & 0x0FU);
            const uint16_t control = (uint16_t)((r1 >> 4) & 0x3FFU);
            const uint32_t epoch = hitag2_self_test_rand(&seed) & 0x3FFFFU;

            const uint32_t reference =
                hitag2_self_test_reference_auth(uid, button, control, key, epoch);
            const uint32_t mine = hitag2_fiat_full_auth(uid, button, control, key, epoch);
            if(mine != reference) {
                return false;
            }

            const Hitag2State state31 =
                hitag2_fiat_init_phase(uid, button, control, key, epoch);
            uint8_t recovered[6];
            const bool consistent = hitag2_fiat_invert_init(
                state31, uid, button, control, epoch, recovered);
            if(!consistent) {
                return false;
            }
            if(memcmp(recovered, key, 6) != 0) {
                return false;
            }
        }
    }
    return true;
}
