#include "renault_v1.h"

#include "fiat_v1.h"
#include <lib/subghz/blocks/custom_btn_i.h>
#include <lib/toolbox/manchester_decoder.h>
#include <stdlib.h>
#include <string.h>

#define TAG "RenaultV1Protocol"

#define HITAG2_TE_US           125U
#define HITAG2_HEADER_LOW_US   1500U
#define HITAG2_HEADER_HIGH_US  1000U
#define HITAG2_SHORT_GAP_US    21500U
#define HITAG2_PREAMBLE_PAIRS  250U
#define HITAG2_LONG_FRAMES     3U
#define HITAG2_SHORT_FRAMES    3U
#define HITAG2_UPLOAD_CAPACITY 1295U
#define HITAG2_MIN_COUNT_BIT     88U
#define HITAG2_HEADER_BITS       16U
#define HITAG2_KEY_BITS          64U
#define HITAG2_KEY2_BITS         24U
#define HITAG2_SHORT_KEY_BITS    10U
#define HITAG2_KEY_END_BITS      (HITAG2_HEADER_BITS + HITAG2_KEY_BITS)
#define HITAG2_LONG_FRAME_BITS   (HITAG2_KEY_END_BITS + HITAG2_KEY2_BITS)
#define HITAG2_RECOVERED_YES     1U
#define HITAG2_RECOVERED_BF_MISS 2U
#define HITAG2_KEY_FIELD         "Hitag2 Key"
#define HITAG2_EPOCH_FIELD       "Hitag2 Epoch"

#define HITAG2_HEADER_LOW_MIN_US  1150U
#define HITAG2_HEADER_LOW_MAX_US  2200U
#define HITAG2_HEADER_HIGH_MIN_US 800U
#define HITAG2_HEADER_HIGH_MAX_US 1150U
#define HITAG2_DATA_IGNORE_US     49U
#define HITAG2_DATA_RESET_US      520U
#define HITAG2_TE_HIGH_INIT_US    120U
#define HITAG2_TE_LOW_INIT_US     150U
#define HITAG2_HOP_FIELD          "Hop"

#define FF_SERIAL                 "Serial"
#define FF_BTN                    "Btn"
#define FF_CNT                    "Cnt"
#define FF_KEY                    "Key"

static const SubGhzBlockConst renault_v1_const = {
    .te_short = HITAG2_TE_US,
    .te_long = HITAG2_TE_US * 2U,
    .te_delta = 50,
    .min_count_bit_for_found = HITAG2_MIN_COUNT_BIT,
};

struct SubGhzProtocolDecoderRenaultV1 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    ManchesterState manchester_state;
    uint16_t te_high;
    uint16_t te_low;
    uint16_t header;
    uint8_t recovered;
    uint8_t hitag2_key[6];
    uint32_t hop;
    uint8_t tail_bits;
    bool hitag2_key_valid;
    uint64_t last_data;
    uint64_t last_data_2;
    bool last_frame_valid;
    uint32_t seed;
    uint64_t data_2;
};

struct SubGhzProtocolEncoderRenaultV1 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t seed;
    uint64_t data_2;
    uint8_t recovered;
    uint8_t hitag2_key[6];
    uint8_t tail_bits;
    bool hitag2_key_valid;
};

typedef enum {
    RenaultV1DecoderStepReset = 0,
    RenaultV1DecoderStepCheckSync = 2,
    RenaultV1DecoderStepData = 3,
} RenaultV1DecoderStep;

static void subghz_protocol_decoder_renault_v1_free(void* context) {
    furi_assert(context);
    free(context);
}

const SubGhzProtocolDecoder renault_v1_decoder = {
    .alloc = subghz_protocol_decoder_renault_v1_alloc,
    .free = subghz_protocol_decoder_renault_v1_free,

    .feed = subghz_protocol_decoder_renault_v1_feed,
    .reset = subghz_protocol_decoder_renault_v1_reset,

    .get_hash_data = subghz_protocol_decoder_renault_v1_get_hash_data,
    .serialize = subghz_protocol_decoder_renault_v1_serialize,
    .deserialize = subghz_protocol_decoder_renault_v1_deserialize,
    .get_string = subghz_protocol_decoder_renault_v1_get_string,
};

const SubGhzProtocolEncoder renault_v1_encoder = {
    .alloc = subghz_protocol_encoder_renault_v1_alloc,
    .free = subghz_protocol_encoder_renault_v1_free,
    .deserialize = subghz_protocol_encoder_renault_v1_deserialize,
    .stop = subghz_protocol_encoder_renault_v1_stop,
    .yield = subghz_protocol_encoder_renault_v1_yield,
};

const SubGhzProtocol renault_v1_protocol = {
    .name = RENAULT_PROTOCOL_V1_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_868 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,

    .decoder = &renault_v1_decoder,
    .encoder = &renault_v1_encoder,
};

static const char* hitag2_get_button_name(uint8_t btn) {
    static const char* const names[] = {
        "Sync",
        "Lock",
        "Unlock",
        "??",
        "Trunk",
        "??",
        "??",
        "??",
        "Panic",
    };
    return (btn < COUNT_OF(names)) ? names[btn] : "??";
}

static void hitag2_u64_to_bytes_be(uint64_t value, uint8_t* out, size_t nbytes) {
    for(size_t i = 0; i < nbytes; i++) {
        out[i] = (uint8_t)(value >> (8U * (nbytes - 1U - i)));
    }
}

static uint64_t hitag2_bytes_to_u64_be(const uint8_t* data, size_t nbytes) {
    uint64_t value = 0;
    for(size_t i = 0; i < nbytes; i++) {
        value = (value << 8U) | data[i];
    }
    return value;
}

static void hitag2_pack_key_bytes(uint64_t key, uint64_t key_2, uint8_t raw[11]) {
    hitag2_u64_to_bytes_be(key, raw, 8);
    raw[8] = (uint8_t)(key_2 >> 16U);
    raw[9] = (uint8_t)(key_2 >> 8U);
    raw[10] = (uint8_t)key_2;
}

static uint8_t hitag2_frame_xor(const uint8_t raw[11]) {
    uint8_t value = 0;
    for(size_t i = 0; i < 10; i++) {
        value ^= raw[i];
    }
    return value;
}

static uint8_t hitag2_i4(uint64_t x, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint8_t)((((x >> a) & 1U) << 3U) | (((x >> b) & 1U) << 2U) | (((x >> c) & 1U) << 1U) |
                     ((x >> d) & 1U));
}

static uint8_t hitag2_f20(uint64_t state) {
    const uint8_t s0 = (uint8_t)((0x3C65U >> hitag2_i4(state, 2U, 3U, 5U, 6U)) & 1U);
    const uint8_t s1 = (uint8_t)((0x0EE5U >> hitag2_i4(state, 8U, 12U, 14U, 15U)) & 1U);
    const uint8_t s2 = (uint8_t)((0x0EE5U >> hitag2_i4(state, 17U, 21U, 23U, 26U)) & 1U);
    const uint8_t s3 = (uint8_t)((0x0EE5U >> hitag2_i4(state, 28U, 29U, 31U, 33U)) & 1U);
    const uint8_t s4 = (uint8_t)((0x3C65U >> hitag2_i4(state, 34U, 43U, 44U, 46U)) & 1U);
    return (uint8_t)((0x0DD3929BUL >> ((s0 << 4U) | (s1 << 3U) | (s2 << 2U) | (s3 << 1U) | s4)) &
                     1U);
}

static uint64_t hitag2_lfsr(uint64_t state) {
    const uint64_t fb =
        (state ^ (state >> 2U) ^ (state >> 3U) ^ (state >> 6U) ^ (state >> 7U) ^ (state >> 8U) ^
         (state >> 16U) ^ (state >> 22U) ^ (state >> 23U) ^ (state >> 26U) ^ (state >> 30U) ^
         (state >> 41U) ^ (state >> 42U) ^ (state >> 43U) ^ (state >> 46U) ^ (state >> 47U)) &
        1ULL;
    return (state >> 1U) | (fb << 47U);
}

static uint64_t hitag2_key_to_u64(const uint8_t key[6]) {
    uint64_t key64 = 0ULL;
    for(size_t i = 0; i < 6U; i++) {
        key64 = (key64 << 8U) | key[i];
    }
    return key64;
}

static uint32_t __attribute__((unused))
    hitag2_authenticator(uint32_t uid, uint8_t button, uint32_t counter, const uint8_t key[6]) {
    const uint64_t key64 = hitag2_key_to_u64(key);
    const uint32_t nonce = (counter << 4U) | ((uint32_t)button & 0x0FU);
    uint64_t state = 0ULL;
    for(uint8_t i = 32U; i < 48U; i++) {
        state = (state << 1U) | ((key64 >> i) & 1ULL);
    }
    for(uint8_t i = 0U; i < 32U; i++) {
        state = (state << 1U) | ((uint64_t)((uid >> i) & 1U));
    }
    for(uint8_t i = 0U; i < 32U; i++) {
        const uint64_t nonce_bit = (uint64_t)hitag2_f20(state) ^ ((nonce >> (31U - i)) & 1U);
        state = (state >> 1U) | (((nonce_bit ^ ((key64 >> (31U - i)) & 1ULL)) & 1ULL) << 47U);
    }
    uint32_t hop = 0U;
    for(uint8_t i = 0U; i < 32U; i++) {
        hop = (hop << 1U) | hitag2_f20(state);
        state = hitag2_lfsr(state);
    }
    return hop;
}

static uint16_t hitag2_frame_cnt10(const uint8_t raw[11]) {
    return (uint16_t)(((uint16_t)(raw[4] & 0x0FU) << 6U) | (raw[5] >> 2U));
}

static uint32_t hitag2_frame_hop(const uint8_t raw[11]) {
    return ((uint32_t)(raw[5] & 3U) << 30U) | ((uint32_t)raw[6] << 22U) |
           ((uint32_t)raw[7] << 14U) | ((uint32_t)raw[8] << 6U) | (raw[9] >> 2U);
}

static uint8_t hitag2_frame_tail(const uint8_t raw[11]) {
    return (uint8_t)(raw[9] & 3U);
}

static void hitag2_pack_auth_frame(
    uint32_t uid,
    uint8_t btn,
    uint16_t cnt10,
    uint32_t hop,
    uint8_t tail,
    uint8_t raw[11]) {
    raw[0] = (uint8_t)(uid >> 24U);
    raw[1] = (uint8_t)(uid >> 16U);
    raw[2] = (uint8_t)(uid >> 8U);
    raw[3] = (uint8_t)uid;
    raw[4] = (uint8_t)(((btn & 0x0FU) << 4U) | ((cnt10 >> 6U) & 0x0FU));
    raw[5] = (uint8_t)(((cnt10 & 0x3FU) << 2U) | ((hop >> 30U) & 3U));
    raw[6] = (uint8_t)(hop >> 22U);
    raw[7] = (uint8_t)(hop >> 14U);
    raw[8] = (uint8_t)(hop >> 6U);
    raw[9] = (uint8_t)(((hop << 2U) & 0xFCU) | (tail & 3U));
    raw[10] = hitag2_frame_xor(raw);
}

static void hitag2_apply_raw(uint8_t raw[11], uint64_t* data, uint64_t* data_2) {
    *data = hitag2_bytes_to_u64_be(raw, 8);
    *data_2 = ((uint64_t)raw[8] << 16U) | ((uint64_t)raw[9] << 8U) | raw[10];
}

static bool hitag2_key_nonzero(const uint8_t key[6]) {
    for(size_t i = 0; i < 6U; i++) {
        if(key[i]) return true;
    }
    return false;
}

static void hitag2_flipper_u32(FlipperFormat* ff, const char* key, uint32_t value) {
    flipper_format_rewind(ff);
    if(!flipper_format_update_uint32(ff, key, &value, 1)) {
        flipper_format_rewind(ff);
        flipper_format_insert_or_update_uint32(ff, key, &value, 1);
    }
}

static bool hitag2_read_hex_be(
    FlipperFormat* flipper_format,
    const char* name,
    uint8_t* data,
    size_t nbytes);

static bool hitag2_read_key(FlipperFormat* ff, uint8_t key[6]) {
    memset(key, 0, 6U);
    if(!hitag2_read_hex_be(ff, HITAG2_KEY_FIELD, key, 6U)) {
        return false;
    }
    return hitag2_key_nonzero(key);
}

static bool hitag2_key_matches_hop(
    const uint8_t key[6],
    uint32_t uid,
    uint8_t btn,
    uint16_t cnt10,
    uint32_t hop) {
    return subghz_protocol_fiat_v1_compute_auth(uid, btn, cnt10 & 0x3FFU, key, 0U) == hop;
}

static bool hitag2_find_known_key(
    uint32_t uid,
    uint8_t btn,
    uint16_t cnt10,
    uint32_t hop,
    uint8_t out_key[6]) {
    const uint8_t(*known_keys)[6] = subghz_protocol_fiat_v1_get_known_keys();
    for(uint8_t i = 0; i < FIAT_V1_KNOWN_KEY_COUNT; i++) {
        if(subghz_protocol_fiat_v1_compute_auth(uid, btn, cnt10 & 0x3FFU, known_keys[i], 0U) ==
           hop) {
            memcpy(out_key, known_keys[i], 6U);
            return true;
        }
    }
    return false;
}

static void hitag2_unpack_frame(
    uint64_t data,
    uint64_t data_2,
    uint32_t* serial,
    uint8_t* btn,
    uint16_t* cnt10,
    uint32_t* hop,
    uint8_t* tail) {
    uint8_t raw[11];
    hitag2_pack_key_bytes(data, data_2, raw);
    if(serial) {
        *serial = (uint32_t)(data >> 32U);
    }
    if(btn) {
        *btn = (uint8_t)((raw[4] >> 4U) & 0x0FU);
    }
    if(cnt10) {
        *cnt10 = hitag2_frame_cnt10(raw);
    }
    if(hop) {
        *hop = hitag2_frame_hop(raw);
    }
    if(tail) {
        *tail = hitag2_frame_tail(raw);
    }
}

static bool hitag2_read_hex_be(
    FlipperFormat* flipper_format,
    const char* name,
    uint8_t* data,
    size_t nbytes) {
    if(!flipper_format_rewind(flipper_format)) {
        return false;
    }
    return flipper_format_read_hex(flipper_format, name, data, nbytes);
}

static bool hitag2_write_hex_be(
    FlipperFormat* flipper_format,
    const char* name,
    uint64_t value,
    size_t nbytes) {
    uint8_t data[8] = {0};
    furi_check(nbytes <= sizeof(data));
    hitag2_u64_to_bytes_be(value, data, nbytes);
    return flipper_format_insert_or_update_hex(flipper_format, name, data, nbytes);
}

static uint8_t hitag2_extract_bits(uint32_t value, uint8_t lsb, uint8_t width) {
    return (uint8_t)((value >> lsb) & ((1U << width) - 1U));
}

static void hitag2_serial_permute(const uint8_t serial[4], uint8_t perm[6]) {
    const uint8_t sn0 = serial[0];
    const uint8_t sn1 = serial[1];
    const uint8_t sn2 = serial[2];
    const uint8_t sn3 = serial[3];

    uint8_t acc =
        (uint8_t)(((sn0 >> 6) & 2U) | ((sn1 >> 4) & 8U) | hitag2_extract_bits(sn0 ^ 0x10U, 4, 1));
    acc |= (uint8_t)((~(uint32_t)(sn0 << 2)) & 0x20U);
    acc |= (uint8_t)((sn0 << 5) & 0x40U);
    acc |= (uint8_t)((sn2 << 1) & 0x80U);
    acc |= (uint8_t)((~(uint32_t)(sn2 >> 5)) & 4U);
    acc |= (uint8_t)((~(uint32_t)(sn0 >> 2)) & 0x10U);
    perm[0] = acc;

    const uint8_t sn1_inv_shr3 = (uint8_t) ~(sn1 >> 3);
    const uint8_t sn3_inv_shl3 = (uint8_t) ~(sn3 << 3);
    acc = (uint8_t)(hitag2_extract_bits(sn1, 5, 1) | (sn1_inv_shr3 & 4U) | (sn0 & 0x20U));
    acc |= (uint8_t)(sn3_inv_shl3 & 8U);
    acc |= (uint8_t)((~(uint32_t)(sn2 << 2)) & 0x10U);
    acc |= (uint8_t)((~(uint32_t)(sn2 << 3)) & 0x40U);
    acc |= 0x80U;
    perm[1] = acc;

    const uint8_t sn0_shr3 = (uint8_t)(sn0 >> 3);
    acc = (uint8_t)(hitag2_extract_bits(sn0, 2, 1) | (sn0_shr3 & 2U) |
                    ((~(uint32_t)(sn3 >> 2)) & 4U) | (sn1 & 0x10U));
    acc |= (uint8_t)((~(uint32_t)(sn1 << 4)) & 0x20U);
    acc |= (uint8_t)(sn3_inv_shl3 & 0x40U);
    acc |= 0x80U;
    perm[2] = acc;

    const uint8_t sn2_inv_shl6 = (uint8_t) ~(sn2 << 6);
    acc =
        (uint8_t)(((sn0 >> 2) & 2U) | ((sn1 >> 3) & 8U) | hitag2_extract_bits(sn2 ^ 0x20U, 5, 1));
    acc |= (uint8_t)((sn1 << 5) & 0x20U);
    acc |= (uint8_t)((sn3 << 1) & 0x40U);
    acc |= (uint8_t)(((uint8_t)~sn0_shr3) & 4U);
    acc |= (uint8_t)(sn1_inv_shr3 & 0x10U);
    acc |= (uint8_t)(sn2_inv_shl6 & 0x80U);
    perm[3] = acc;

    uint8_t perm4_lo =
        (uint8_t)(((sn3 << 2) & 8U) | ((sn0 << 4) & 0x10U) | hitag2_extract_bits(sn0 ^ 4U, 2, 1));
    perm4_lo |= (uint8_t)((~(uint32_t)(sn3 >> 1)) & 2U);
    perm4_lo |= (uint8_t)((~(uint32_t)(sn1 >> 4)) & 4U);
    uint8_t perm4_hi = (uint8_t)((~(uint32_t)(sn0 << 4)) & 0x20U);
    perm4_hi |= (uint8_t)(sn2_inv_shl6 & 0x40U);
    const uint8_t sn1_inv_shl3 = (uint8_t) ~(sn1 << 3);
    perm4_hi |= (uint8_t)(sn1_inv_shl3 & 0x80U);
    perm[4] = (uint8_t)(perm4_lo | perm4_hi);

    uint8_t perm5 =
        (uint8_t)(((sn3 >> 2) & 0x10U) | ((sn2 >> 3) & 2U) | (((uint8_t)~sn0) & 0x80U));
    perm5 |= (uint8_t)((~(uint32_t)(sn0 << 3)) & 8U);
    perm5 |= (uint8_t)((sn0 >> 2) & 0x10U);
    perm5 |= (uint8_t)(sn1_inv_shl3 & 0x20U);
    perm5 |= (uint8_t)((sn3 >> 1) & 0x40U);
    perm5 |= (uint8_t)((~(uint32_t)(sn1 >> 1)) & 4U);
    perm[5] = perm5;
}

static uint8_t hitag2_truth(uint32_t table, uint8_t index) {
    return (uint8_t)((table >> index) & 1U);
}

static uint8_t hitag2_filter_index(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (uint8_t)((a << 3U) | (b << 2U) | (c << 1U) | d);
}

static uint8_t hitag2_byte_bit(uint8_t byte, uint8_t bit) {
    return (uint8_t)((byte >> bit) & 1U);
}

static uint8_t hitag2_filter(const uint8_t state[6]) {
    uint8_t group = 0;
    group |= hitag2_truth(
        0x2C79U,
        hitag2_filter_index(
            hitag2_byte_bit(state[0], 1),
            hitag2_byte_bit(state[0], 2),
            hitag2_byte_bit(state[0], 4),
            hitag2_byte_bit(state[0], 5)));
    group |= (uint8_t)(hitag2_truth(
                           0x6671U,
                           hitag2_filter_index(
                               hitag2_byte_bit(state[1], 0),
                               hitag2_byte_bit(state[1], 1),
                               hitag2_byte_bit(state[1], 3),
                               hitag2_byte_bit(state[1], 7)))
                       << 1U);
    group |= (uint8_t)(hitag2_truth(
                           0x6671U,
                           hitag2_filter_index(
                               hitag2_byte_bit(state[3], 5),
                               hitag2_byte_bit(state[2], 0),
                               hitag2_byte_bit(state[2], 2),
                               hitag2_byte_bit(state[2], 6)))
                       << 2U);
    group |= (uint8_t)(hitag2_truth(
                           0x6671U,
                           hitag2_filter_index(
                               hitag2_byte_bit(state[4], 6),
                               hitag2_byte_bit(state[3], 0),
                               hitag2_byte_bit(state[3], 2),
                               hitag2_byte_bit(state[3], 3)))
                       << 3U);
    group |= (uint8_t)(hitag2_truth(
                           0x2C79U,
                           hitag2_filter_index(
                               hitag2_byte_bit(state[5], 1),
                               hitag2_byte_bit(state[5], 3),
                               hitag2_byte_bit(state[5], 4),
                               hitag2_byte_bit(state[4], 5)))
                       << 4U);
    return hitag2_truth(0x7907287BUL, group);
}

static uint8_t hitag2_parity8(uint8_t value) {
    value ^= (uint8_t)(value >> 4U);
    value ^= (uint8_t)(value >> 2U);
    value ^= (uint8_t)(value >> 1U);
    return (uint8_t)(value & 1U);
}

static uint8_t hitag2_feedback(const uint8_t state[6]) {
    static const uint8_t masks[6] = {0xB3U, 0x80U, 0x83U, 0x22U, 0x00U, 0x73U};
    uint8_t feedback = 0;
    for(uint8_t i = 0; i < 6; i++) {
        feedback ^= hitag2_parity8((uint8_t)(state[i] & masks[i]));
    }
    return (uint8_t)(feedback & 1U);
}

static void hitag2_shift_state(uint8_t state[6], uint8_t input) {
    for(uint8_t i = 0; i < 5; i++) {
        state[i] = (uint8_t)((state[i] << 1U) | (state[i + 1U] >> 7U));
    }
    state[5] = (uint8_t)((state[5] << 1U) | (input & 1U));
}

static void hitag2_shift_u32(uint8_t buf[4], uint8_t inject) {
    const uint8_t b0 = buf[0];
    const uint8_t b1 = buf[1];
    const uint8_t b2 = buf[2];
    const uint8_t b3 = buf[3];
    buf[3] = (uint8_t)((b2 >> 7U) | (b3 << 1U));
    buf[2] = (uint8_t)((b1 >> 7U) | (b2 << 1U));
    buf[1] = (uint8_t)((b0 >> 7U) | (b1 << 1U));
    buf[0] = (uint8_t)((b0 << 1U) | (inject & 1U));
}

static void hitag2_clock_cipher(uint8_t state[6], uint8_t iv_work[4], uint8_t iv_orig[4]) {
    for(uint8_t i = 0; i < 32; i++) {
        const uint8_t filter = hitag2_filter(state);
        uint8_t mix = (iv_work[3] & 0x80U) ? (filter ? 0U : 1U) : (filter ? 1U : 0U);
        if(iv_orig[3] & 0x80U) {
            mix ^= 5U;
        }
        hitag2_shift_state(state, (uint8_t)(mix & 1U));
        hitag2_shift_u32(iv_work, 0);
        hitag2_shift_u32(iv_orig, (uint8_t)((mix >> 2U) & 1U));
    }

    for(uint8_t i = 0; i < 32; i++) {
        hitag2_shift_u32(iv_work, 0);
        if(hitag2_filter(state)) {
            iv_work[0] |= 1U;
        }
        hitag2_shift_state(state, hitag2_feedback(state));
    }
}

static void hitag2_build_iv(uint32_t cnt, uint8_t btn, uint32_t seed, uint8_t iv[4]) {
    iv[0] = (uint8_t)(((cnt << 4U) & 0xF0U) | (btn & 0x0FU));
    iv[1] = (uint8_t)((cnt >> 4U) & 0xFFU);
    iv[2] = (uint8_t)(((seed >> 8U) & 0xF0U) | ((cnt >> 12U) & 0x0FU));
    iv[3] = (uint8_t)(seed & 0xFFU);
}

static void
    hitag2_encrypt_from_iv(const uint8_t serial_be[4], const uint8_t iv[4], uint8_t out[11]) {
    uint8_t perm[6];
    uint8_t state[6];
    uint8_t iv_work[4];
    uint8_t iv_orig[4];

    hitag2_serial_permute(serial_be, perm);
    state[0] = serial_be[0];
    state[1] = serial_be[1];
    state[2] = serial_be[2];
    state[3] = serial_be[3];
    state[4] = perm[4];
    state[5] = perm[5];
    iv_work[0] = perm[0];
    iv_work[1] = perm[1];
    iv_work[2] = perm[2];
    iv_work[3] = perm[3];
    iv_orig[0] = iv[0];
    iv_orig[1] = iv[1];
    iv_orig[2] = iv[2];
    iv_orig[3] = iv[3];
    hitag2_clock_cipher(state, iv_work, iv_orig);

    const uint8_t hop0 = iv_work[0];
    uint8_t hop1 = (uint8_t)((hop0 >> 7U) | (iv_work[1] << 1U));
    uint8_t hop2 = (uint8_t)((iv_work[1] >> 7U) | (iv_work[2] << 1U));
    const uint8_t hop_ext = (uint8_t)(((hop0 >> 6U) & 1U) | (hop1 << 1U));
    uint8_t hop3 = (uint8_t)((iv_work[2] >> 7U) | (iv_work[3] << 1U));
    uint8_t hop4 = (uint8_t)((iv_work[3] >> 7U) | (state[5] << 1U));
    hop1 = (uint8_t)((hop1 >> 7U) | (hop2 << 1U));
    hop2 = (uint8_t)((hop2 >> 7U) | (hop3 << 1U));
    hop3 = (uint8_t)((hop3 >> 7U) | (hop4 << 1U));

    uint32_t mix = ((uint32_t)iv[1] << 4U) | ((uint32_t)iv[0] >> 4U);
    mix = (mix | (((uint32_t)iv[2] << 12U) & 0xFFFFU)) & 0xFFFFU;

    out[0] = serial_be[0];
    out[1] = serial_be[1];
    out[2] = serial_be[2];
    out[3] = serial_be[3];
    out[4] = (uint8_t)(((mix >> 6U) & 0x0FU) | ((iv[0] << 4U) & 0xF0U));
    out[5] = (uint8_t)((hop3 & 3U) | ((mix << 2U) & 0xFCU));
    out[6] = hop2;
    out[7] = hop1;
    out[8] = hop_ext;
    out[9] = (uint8_t)((hop0 << 2U) | 2U);
    out[10] = hitag2_frame_xor(out);
}

static void hitag2_encrypt_frame(
    uint32_t serial,
    uint32_t cnt,
    uint8_t btn,
    uint32_t seed,
    uint8_t out[11],
    uint8_t iv[4]) {
    const uint8_t serial_be[4] = {
        (uint8_t)(serial >> 24U),
        (uint8_t)(serial >> 16U),
        (uint8_t)(serial >> 8U),
        (uint8_t)serial,
    };
    hitag2_build_iv(cnt, btn, seed, iv);
    hitag2_encrypt_from_iv(serial_be, iv, out);
}

#define HITAG2_SEED_BF_CANDIDATES 0x10000U
#define HITAG2_SEED_BF_YIELD_STEP 0x1000U

bool subghz_protocol_renault_v1_run_seed_bf_ex(
    uint64_t data,
    uint64_t data_2,
    uint32_t serial,
    uint8_t btn,
    uint32_t cnt,
    uint32_t* seed_out,
    Hitag2SeedProgressCallback progress_cb,
    void* progress_ctx) {
    uint8_t captured_raw[11];
    hitag2_pack_key_bytes(data, data_2, captured_raw);

    uint8_t out[11];
    uint8_t iv[4];
    for(uint32_t cand = 0; cand < HITAG2_SEED_BF_CANDIDATES; cand++) {

        if(progress_cb && ((cand & (HITAG2_SEED_BF_YIELD_STEP - 1U)) == 0U)) {
            uint8_t pct = (uint8_t)(((uint64_t)cand * 100U) / HITAG2_SEED_BF_CANDIDATES);
            if(!progress_cb(pct, cand, progress_ctx)) {
                return false;
            }
        }

        hitag2_encrypt_frame(serial, cnt, btn, cand, out, iv);
        if(memcmp(out, captured_raw, sizeof(out)) == 0) {
            if(seed_out) {
                *seed_out = ((uint32_t)iv[0] << 24U) | ((uint32_t)iv[1] << 16U) |
                            ((uint32_t)iv[2] << 8U) | iv[3];
            }
            return true;
        }
    }
    return false;
}

bool subghz_protocol_renault_v1_run_seed_bf(
    uint64_t data,
    uint64_t data_2,
    uint32_t serial,
    uint8_t btn,
    uint32_t cnt,
    uint32_t* seed_out) {
    return subghz_protocol_renault_v1_run_seed_bf_ex(
        data, data_2, serial, btn, cnt, seed_out, NULL, NULL);
}

static void hitag2_apply_check_remote(
    uint64_t data,
    uint64_t data_2,
    uint8_t recovered,
    uint32_t seed,
    uint32_t* serial,
    uint8_t* btn,
    uint32_t* cnt,
    uint32_t* hop,
    uint8_t* tail) {
    uint16_t cnt10 = 0;
    hitag2_unpack_frame(data, data_2, serial, btn, &cnt10, hop, tail);
    if(recovered == HITAG2_RECOVERED_YES) {
        *cnt = ((seed >> 12U) & 0xFF0U) | ((seed << 4U) & 0xF000U) | (seed >> 28U);
    } else {
        *cnt = cnt10;
    }
}

static void renault_v1_check_remote_controller(SubGhzProtocolDecoderRenaultV1* instance) {
    hitag2_apply_check_remote(
        instance->generic.data,
        instance->data_2,
        instance->recovered,
        instance->seed,
        &instance->generic.serial,
        &instance->generic.btn,
        &instance->generic.cnt,
        &instance->hop,
        &instance->tail_bits);
}

static bool hitag2_encoder_next_frame(
    SubGhzProtocolEncoderRenaultV1* instance,
    uint32_t orig_uid,
    uint8_t orig_btn,
    uint16_t orig_cnt10,
    uint32_t orig_hop,
    uint8_t tail) {
    uint8_t raw[11];
    if(!hitag2_key_matches_hop(instance->hitag2_key, orig_uid, orig_btn, orig_cnt10, orig_hop)) {
        return false;
    }

    uint8_t tx_btn = instance->generic.btn;
    if(tx_btn == 0U) {
        tx_btn = orig_btn;
    }
    uint32_t uid = instance->generic.serial ? instance->generic.serial : orig_uid;
    uint32_t mult = furi_hal_subghz_get_rolling_counter_mult();
    if(mult == 0U) mult = 1U;
    uint16_t cnt10 = (uint16_t)((instance->generic.cnt + mult) & 0x3FFU);
    uint32_t hop = subghz_protocol_fiat_v1_compute_auth(uid, tx_btn, cnt10, instance->hitag2_key, 0U);

    hitag2_pack_auth_frame(uid, tx_btn, cnt10, hop, tail, raw);
    hitag2_apply_raw(raw, &instance->generic.data, &instance->data_2);
    instance->generic.serial = uid;
    instance->generic.btn = tx_btn;
    instance->generic.cnt = cnt10;
    instance->tail_bits = tail;
    instance->hitag2_key_valid = true;
    return true;
}

static void hitag2_write_named_fields(
    FlipperFormat* ff,
    uint32_t serial,
    uint8_t btn,
    uint32_t cnt,
    uint32_t hop) {
    hitag2_flipper_u32(ff, FF_SERIAL, serial);
    hitag2_flipper_u32(ff, FF_BTN, btn);
    hitag2_flipper_u32(ff, FF_CNT, cnt);
    hitag2_flipper_u32(ff, HITAG2_HOP_FIELD, hop);
}

static void hitag2_read_recovered_and_seed(
    FlipperFormat* flipper_format,
    uint8_t* recovered,
    uint32_t* seed) {
    *recovered = 0;
    *seed = 0;

    uint8_t recovered_hex = 0;
    if(hitag2_read_hex_be(flipper_format, "Recovered", &recovered_hex, 1)) {
        *recovered = recovered_hex;
    } else {
        uint32_t recovered_u32 = 0;
        if(flipper_format_rewind(flipper_format) &&
           flipper_format_read_uint32(flipper_format, "Recovered", &recovered_u32, 1)) {
            *recovered = (uint8_t)recovered_u32;
        }
    }

    uint8_t seed_be[4] = {0};
    if(hitag2_read_hex_be(flipper_format, "Seed", seed_be, 4)) {
        *seed = (uint32_t)hitag2_bytes_to_u64_be(seed_be, 4);
        return;
    }

    if(flipper_format_rewind(flipper_format)) {
        flipper_format_read_uint32(flipper_format, "Seed", seed, 1);
    }
}

void* subghz_protocol_encoder_renault_v1_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderRenaultV1* instance = calloc(1, sizeof(SubGhzProtocolEncoderRenaultV1));
    furi_check(instance);

    instance->base.protocol = &renault_v1_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat = 1;
    instance->encoder.front = 0;
    instance->encoder.is_running = false;
    instance->encoder.upload = malloc(HITAG2_UPLOAD_CAPACITY * sizeof(LevelDuration));
    furi_check(instance->encoder.upload);

    return instance;
}

void subghz_protocol_encoder_renault_v1_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderRenaultV1* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

void subghz_protocol_encoder_renault_v1_stop(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderRenaultV1* instance = context;
    instance->encoder.is_running = false;
    instance->encoder.front = 0;
}

LevelDuration subghz_protocol_encoder_renault_v1_yield(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderRenaultV1* instance = context;

    if(!instance->encoder.is_running || instance->encoder.repeat == 0 ||
       instance->encoder.size_upload == 0) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        if(!subghz_block_generic_global.endless_tx) instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}

static bool
    hitag2_encoder_add_level(LevelDuration* upload, size_t* index, bool level, uint32_t duration) {
    if(*index >= HITAG2_UPLOAD_CAPACITY) {
        return false;
    }
    upload[(*index)++] = level_duration_make(level, duration);
    return true;
}

static bool hitag2_encoder_add_bits(
    LevelDuration* upload,
    size_t* index,
    uint64_t value,
    uint8_t bit_count) {
    for(uint8_t i = bit_count; i > 0; i--) {
        const bool one = ((value >> (i - 1U)) & 1ULL) != 0;
        if(one) {
            if(!hitag2_encoder_add_level(upload, index, true, HITAG2_TE_US)) return false;
            if(!hitag2_encoder_add_level(upload, index, false, HITAG2_TE_US)) return false;
        } else {
            if(!hitag2_encoder_add_level(upload, index, false, HITAG2_TE_US)) return false;
            if(!hitag2_encoder_add_level(upload, index, true, HITAG2_TE_US)) return false;
        }
    }
    return true;
}

static bool renault_v1_encoder_get_upload(SubGhzProtocolEncoderRenaultV1* instance) {
    furi_check(instance);

    size_t index = 0;
    LevelDuration* upload = instance->encoder.upload;
    const uint64_t key = instance->generic.data;
    const uint64_t key_2 = instance->data_2 & 0xFFFFFFULL;

    for(size_t i = 0; i < HITAG2_PREAMBLE_PAIRS; i++) {
        if(!hitag2_encoder_add_level(upload, &index, true, HITAG2_TE_US)) return false;
        if(!hitag2_encoder_add_level(upload, &index, false, HITAG2_TE_US)) return false;
    }

    for(uint8_t frame = 0; frame < HITAG2_LONG_FRAMES; frame++) {
        if(!hitag2_encoder_add_level(upload, &index, false, HITAG2_HEADER_LOW_US)) return false;
        if(!hitag2_encoder_add_level(upload, &index, true, HITAG2_HEADER_HIGH_US)) return false;
        if(!hitag2_encoder_add_bits(upload, &index, 1, HITAG2_HEADER_BITS)) return false;
        if(!hitag2_encoder_add_bits(upload, &index, key, HITAG2_KEY_BITS)) return false;
        if(!hitag2_encoder_add_bits(upload, &index, key_2, HITAG2_KEY2_BITS)) return false;
    }

    const uint64_t short_key = (key >> 18U) & 0x3FFULL;
    for(uint8_t frame = 0; frame < HITAG2_SHORT_FRAMES; frame++) {
        if(!hitag2_encoder_add_level(upload, &index, false, HITAG2_HEADER_LOW_US)) return false;
        if(!hitag2_encoder_add_level(upload, &index, true, HITAG2_HEADER_HIGH_US)) return false;
        if(!hitag2_encoder_add_bits(upload, &index, 1, HITAG2_HEADER_BITS)) return false;
        if(!hitag2_encoder_add_bits(upload, &index, short_key, HITAG2_SHORT_KEY_BITS))
            return false;
        if(!hitag2_encoder_add_level(upload, &index, false, HITAG2_SHORT_GAP_US)) return false;
    }

    instance->encoder.size_upload = index;
    return true;
}

SubGhzProtocolStatus
    subghz_protocol_encoder_renault_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderRenaultV1* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic, flipper_format, renault_v1_const.min_count_bit_for_found);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }

        uint8_t key2[8] = {0};
        if(!hitag2_read_hex_be(flipper_format, "Key_2", key2, sizeof(key2))) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->data_2 = hitag2_bytes_to_u64_be(key2, sizeof(key2));

        hitag2_read_recovered_and_seed(flipper_format, &instance->recovered, &instance->seed);

        uint8_t custom_btn_original = 0;
        hitag2_unpack_frame(
            instance->generic.data, instance->data_2, NULL, &custom_btn_original, NULL, NULL, NULL);
        if(subghz_custom_btn_get_original() == 0) {
            subghz_custom_btn_set_original(custom_btn_original);
        }
        subghz_custom_btn_set_max(4U);
        switch(subghz_custom_btn_get()) {
        case SUBGHZ_CUSTOM_BTN_UP:
            instance->generic.btn = 0x01U;
            break;
        case SUBGHZ_CUSTOM_BTN_DOWN:
            instance->generic.btn = 0x02U;
            break;
        case SUBGHZ_CUSTOM_BTN_LEFT:
            instance->generic.btn = 0x04U;
            break;
        case SUBGHZ_CUSTOM_BTN_RIGHT:
            instance->generic.btn = 0x08U;
            break;
        case SUBGHZ_CUSTOM_BTN_OK:
        default:
            instance->generic.btn = 0U;
            break;
        }

        if(!flipper_format_rewind(flipper_format)) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }

        uint32_t hop = 0;
        uint8_t tail = 0;
        uint32_t wire_serial = 0;
        uint8_t wire_btn = 0;
        uint16_t wire_cnt10 = 0;
        hitag2_unpack_frame(
            instance->generic.data,
            instance->data_2,
            &wire_serial,
            &wire_btn,
            &wire_cnt10,
            &hop,
            &tail);
        hitag2_apply_check_remote(
            instance->generic.data,
            instance->data_2,
            instance->recovered,
            instance->seed,
            &instance->generic.serial,
            &instance->generic.btn,
            &instance->generic.cnt,
            &hop,
            &tail);
        instance->tail_bits = tail;

        const uint32_t orig_uid = wire_serial;
        const uint8_t orig_btn = wire_btn;
        const uint16_t orig_cnt10 = wire_cnt10;
        const uint32_t orig_hop = hop;

        instance->hitag2_key_valid = false;
        memset(instance->hitag2_key, 0, 6U);
        bool have_hitag2_key = hitag2_read_key(flipper_format, instance->hitag2_key);
        if(!have_hitag2_key) {
            have_hitag2_key = hitag2_find_known_key(
                orig_uid, orig_btn, orig_cnt10, orig_hop, instance->hitag2_key);
        }
        if(have_hitag2_key) {
            if(!hitag2_encoder_next_frame(
                   instance, orig_uid, orig_btn, orig_cnt10, orig_hop, tail) &&
               instance->recovered != HITAG2_RECOVERED_YES) {
                ret = SubGhzProtocolStatusErrorParserOthers;
                break;
            }
        }

        uint32_t tx_repeat = 1U;
        flipper_format_rewind(flipper_format);
        flipper_format_read_uint32(flipper_format, "Repeat", &tx_repeat, 1);
        if(tx_repeat == 0U) {
            tx_repeat = 1U;
        }
        instance->encoder.repeat = tx_repeat;

        if(!instance->hitag2_key_valid && instance->recovered == HITAG2_RECOVERED_YES) {
            uint8_t out[11];
            uint8_t iv[4];
            uint32_t seed_mult = furi_hal_subghz_get_rolling_counter_mult();
            if(seed_mult == 0U) seed_mult = 1U;
            instance->generic.cnt = (instance->generic.cnt + seed_mult) & 0xFFFFU;
            hitag2_encrypt_frame(
                instance->generic.serial,
                instance->generic.cnt,
                instance->generic.btn,
                instance->seed,
                out,
                iv);
            instance->generic.data = hitag2_bytes_to_u64_be(out, 8);
            instance->data_2 = ((uint64_t)out[8] << 16U) | ((uint64_t)out[9] << 8U) | out[10];
            instance->seed = ((uint32_t)iv[0] << 24U) | ((uint32_t)iv[1] << 16U) |
                             ((uint32_t)iv[2] << 8U) | iv[3];
        } else if(!instance->hitag2_key_valid && instance->recovered != HITAG2_RECOVERED_YES) {

        }

        if(!renault_v1_encoder_get_upload(instance)) {
            ret = SubGhzProtocolStatusErrorEncoderGetUpload;
            break;
        }

        if(!flipper_format_rewind(flipper_format)) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        uint8_t key_data[8];
        hitag2_u64_to_bytes_be(instance->generic.data, key_data, 8);
        if(!flipper_format_insert_or_update_hex(
               flipper_format, FF_KEY, key_data, sizeof(key_data))) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        hitag2_u64_to_bytes_be(instance->data_2, key_data, 8);
        if(!flipper_format_insert_or_update_hex(
               flipper_format, "Key_2", key_data, sizeof(key_data))) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        if(instance->recovered == HITAG2_RECOVERED_YES) {
            const uint8_t recovered_hex = HITAG2_RECOVERED_YES;
            if(!flipper_format_insert_or_update_hex(
                   flipper_format, "Recovered", &recovered_hex, 1)) {
                ret = SubGhzProtocolStatusErrorParserOthers;
                break;
            }
            if(!hitag2_write_hex_be(flipper_format, "Seed", instance->seed, 4)) {
                ret = SubGhzProtocolStatusErrorParserOthers;
                break;
            }
        }

        {
            uint32_t hop_out = 0;
            hitag2_unpack_frame(
                instance->generic.data, instance->data_2, NULL, NULL, NULL, &hop_out, NULL);
            hitag2_write_named_fields(
                flipper_format,
                instance->generic.serial,
                instance->generic.btn,
                instance->generic.cnt,
                hop_out);
        }
        if(instance->hitag2_key_valid) {
            flipper_format_rewind(flipper_format);
            flipper_format_insert_or_update_hex(
                flipper_format, HITAG2_KEY_FIELD, instance->hitag2_key, 6U);
            hitag2_flipper_u32(flipper_format, HITAG2_EPOCH_FIELD, 0U);
        }

        instance->encoder.front = 0;
        instance->encoder.is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

void* subghz_protocol_decoder_renault_v1_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderRenaultV1* instance = calloc(1, sizeof(SubGhzProtocolDecoderRenaultV1));
    furi_check(instance);
    instance->base.protocol = &renault_v1_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void subghz_protocol_decoder_renault_v1_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderRenaultV1* instance = context;
    instance->decoder.parser_step = RenaultV1DecoderStepReset;
    manchester_advance(
        instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
}

static bool hitag2_duration_is_header_low(uint32_t duration) {
    return (duration >= HITAG2_HEADER_LOW_MIN_US) && (duration <= HITAG2_HEADER_LOW_MAX_US);
}

static uint32_t hitag2_data_threshold(uint16_t te) {
    const uint32_t triple = (uint32_t)te * 3U;
    if(triple < 300U) {
        return 150U;
    }
    if(triple >= 422U) {
        return 210U;
    }
    return triple / 2U;
}

static uint16_t hitag2_adapt_te(uint16_t te, uint32_t duration) {
    const uint32_t mixed = ((uint32_t)te * 7U) + duration;
    if(mixed < 560U) {
        return 70;
    }
    if(mixed >= 1488U) {
        return 185;
    }
    return (uint16_t)(mixed / 8U);
}

static bool hitag2_accept_frame(SubGhzProtocolDecoderRenaultV1* instance, uint64_t key_2) {
    if(instance->header != 1U) {
        return false;
    }

    uint8_t raw[11];
    hitag2_pack_key_bytes(instance->generic.data, key_2, raw);
    if(hitag2_frame_xor(raw) != raw[10]) {
        return false;
    }

    if(instance->last_frame_valid && instance->last_data == instance->generic.data &&
       instance->last_data_2 == key_2) {
        return false;
    }

    instance->data_2 = key_2;
    instance->generic.data_count_bit = HITAG2_MIN_COUNT_BIT;
    instance->recovered = 0;
    instance->seed = 0;
    uint16_t cnt10 = 0;
    hitag2_unpack_frame(
        instance->generic.data,
        instance->data_2,
        &instance->generic.serial,
        &instance->generic.btn,
        &cnt10,
        &instance->hop,
        &instance->tail_bits);
    instance->generic.cnt = cnt10;
    instance->last_data = instance->generic.data;
    instance->last_data_2 = key_2;
    instance->last_frame_valid = true;
    return true;
}

void subghz_protocol_decoder_renault_v1_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderRenaultV1* instance = context;

    while(true) {
        switch(instance->decoder.parser_step) {
        case RenaultV1DecoderStepReset:
            if((!level) && hitag2_duration_is_header_low(duration)) {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = RenaultV1DecoderStepCheckSync;
            }
            return;

        case RenaultV1DecoderStepCheckSync:
            if(level) {
                if((duration < HITAG2_HEADER_HIGH_MIN_US) ||
                   (duration > HITAG2_HEADER_HIGH_MAX_US) ||
                   (instance->decoder.te_last < HITAG2_HEADER_LOW_MIN_US) ||
                   (instance->decoder.te_last > HITAG2_HEADER_LOW_MAX_US)) {
                    instance->decoder.parser_step = RenaultV1DecoderStepReset;
                    return;
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->manchester_state = ManchesterStateStart1;
                instance->te_high = HITAG2_TE_HIGH_INIT_US;
                instance->te_low = HITAG2_TE_LOW_INIT_US;
                instance->decoder.parser_step = RenaultV1DecoderStepData;
                return;
            }
            instance->decoder.parser_step = RenaultV1DecoderStepReset;
            if(hitag2_duration_is_header_low(duration)) {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = RenaultV1DecoderStepCheckSync;
            }
            return;

        case RenaultV1DecoderStepData:
            if(duration <= HITAG2_DATA_IGNORE_US) {
                return;
            }
            if(duration > HITAG2_DATA_RESET_US) {
                instance->decoder.parser_step = RenaultV1DecoderStepReset;
                continue;
            }

            uint16_t* te = level ? &instance->te_high : &instance->te_low;
            ManchesterEvent event;
            if(duration > hitag2_data_threshold(*te)) {
                event = level ? ManchesterEventLongHigh : ManchesterEventLongLow;
            } else {
                event = level ? ManchesterEventShortHigh : ManchesterEventShortLow;
                *te = hitag2_adapt_te(*te, duration);
            }

            bool bit = false;
            if(!manchester_advance(
                   instance->manchester_state, event, &instance->manchester_state, &bit)) {
                return;
            }

            instance->decoder.decode_data = (instance->decoder.decode_data << 1U) |
                                            (bit ? 1ULL : 0ULL);
            instance->decoder.decode_count_bit++;

            if(instance->decoder.decode_count_bit == HITAG2_HEADER_BITS) {
                instance->header = (uint16_t)~instance->decoder.decode_data;
                instance->decoder.decode_data = 0;
                return;
            }
            if(instance->decoder.decode_count_bit == HITAG2_KEY_END_BITS) {
                instance->generic.data = ~instance->decoder.decode_data;
                instance->decoder.decode_data = 0;
                return;
            }
            if(instance->decoder.decode_count_bit == HITAG2_LONG_FRAME_BITS) {
                const uint64_t key_2 = (~instance->decoder.decode_data) & 0xFFFFFFULL;
                if(hitag2_accept_frame(instance, key_2) && instance->base.callback) {
                    instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = RenaultV1DecoderStepData;
            }
            return;

        default:
            instance->decoder.parser_step = RenaultV1DecoderStepReset;
            return;
        }
    }
}

uint8_t subghz_protocol_decoder_renault_v1_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderRenaultV1* instance = context;
    return (uint8_t)instance->generic.serial ^ (uint8_t)(instance->generic.serial >> 8) ^
           (uint8_t)(instance->generic.serial >> 16) ^ (uint8_t)(instance->generic.serial >> 24) ^
           instance->generic.btn ^ (uint8_t)instance->generic.cnt ^
           (uint8_t)(instance->generic.cnt >> 8);
}

static SubGhzProtocolStatus hitag2_write_extra_fields(
    FlipperFormat* flipper_format,
    uint64_t key_2,
    uint8_t recovered,
    uint32_t seed) {
    if(!hitag2_write_hex_be(flipper_format, "Key_2", key_2, 8)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    uint8_t recovered_hex = 0;
    if(recovered == HITAG2_RECOVERED_YES) {
        recovered_hex = HITAG2_RECOVERED_YES;
    } else if(recovered == HITAG2_RECOVERED_BF_MISS) {
        recovered_hex = HITAG2_RECOVERED_BF_MISS;
    }
    if(!flipper_format_insert_or_update_hex(flipper_format, "Recovered", &recovered_hex, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    if(!hitag2_write_hex_be(flipper_format, "Seed", seed, 4)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    return SubGhzProtocolStatusOk;
}

SubGhzProtocolStatus subghz_protocol_decoder_renault_v1_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderRenaultV1* instance = context;
    renault_v1_check_remote_controller(instance);

    const uint32_t serial = instance->generic.serial;
    const uint8_t btn = instance->generic.btn;
    const uint32_t cnt = instance->generic.cnt;
    const uint32_t seed = instance->seed;
    const uint64_t data_2 = instance->data_2;
    instance->generic.serial = 0;
    instance->generic.btn = 0;
    instance->generic.cnt = 0;
    instance->seed = 0;
    instance->data_2 = 0;
    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    instance->generic.serial = serial;
    instance->generic.btn = btn;
    instance->generic.cnt = cnt;
    instance->seed = seed;
    instance->data_2 = data_2;
    if(ret != SubGhzProtocolStatusOk) {
        return ret;
    }

    ret = hitag2_write_extra_fields(
        flipper_format, instance->data_2, instance->recovered, instance->seed);
    if(ret != SubGhzProtocolStatusOk) {
        return ret;
    }
    hitag2_write_named_fields(flipper_format, serial, btn, cnt, instance->hop);
    if(instance->hitag2_key_valid) {
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_hex(
            flipper_format, HITAG2_KEY_FIELD, instance->hitag2_key, 6U);
        hitag2_flipper_u32(flipper_format, HITAG2_EPOCH_FIELD, 0U);
    }
    return SubGhzProtocolStatusOk;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_renault_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderRenaultV1* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic, flipper_format, renault_v1_const.min_count_bit_for_found);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }

        uint8_t key2[8] = {0};
        if(!hitag2_read_hex_be(flipper_format, "Key_2", key2, sizeof(key2))) {
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        instance->data_2 = hitag2_bytes_to_u64_be(key2, sizeof(key2));

        hitag2_read_recovered_and_seed(flipper_format, &instance->recovered, &instance->seed);

        renault_v1_check_remote_controller(instance);
        instance->hitag2_key_valid = false;
        memset(instance->hitag2_key, 0, 6U);
        if(hitag2_read_key(flipper_format, instance->hitag2_key)) {
            instance->hitag2_key_valid = hitag2_key_matches_hop(
                instance->hitag2_key,
                instance->generic.serial,
                instance->generic.btn,
                (uint16_t)(instance->generic.cnt & 0x3FFU),
                instance->hop);
        }
        if(!instance->hitag2_key_valid) {
            instance->hitag2_key_valid = hitag2_find_known_key(
                instance->generic.serial,
                instance->generic.btn,
                (uint16_t)(instance->generic.cnt & 0x3FFU),
                instance->hop,
                instance->hitag2_key);
        }
        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

void subghz_protocol_decoder_renault_v1_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderRenaultV1* instance = context;

    renault_v1_check_remote_controller(instance);

    furi_string_printf(
        output,
        "%s\r\nK1:%016llX\r\nK2:%06llX Sn:%08lX\r\nBtn:%02X [%s] %db",
        instance->generic.protocol_name,
        (unsigned long long)instance->generic.data,
        (unsigned long long)(instance->data_2 & 0xFFFFFFULL),
        (unsigned long)instance->generic.serial,
        instance->generic.btn,
        hitag2_get_button_name(instance->generic.btn),
        instance->generic.data_count_bit);

    if(instance->recovered == HITAG2_RECOVERED_YES) {
        furi_string_cat_printf(
            output,
            "\r\nIV:%08lX Cnt:%04lX",
            (unsigned long)instance->seed,
            (unsigned long)(instance->generic.cnt & 0xFFFFU));
    } else if(instance->recovered == HITAG2_RECOVERED_BF_MISS) {
        furi_string_cat_printf(output, "\r\nBF not found");
    } else {
        furi_string_cat_printf(
            output,
            "\r\nCnt:%03lX Hop:%08lX",
            (unsigned long)(instance->generic.cnt & 0x3FFU),
            (unsigned long)instance->hop);
        if(instance->hitag2_key_valid) {
            furi_string_cat_printf(output, "\r\nKEY:OK");
        } else {
            furi_string_cat_printf(output, "\r\nKEY:??");
        }
    }
}
