#include "ford_v3.h"
#include <furi.h>
#include <string.h>
#include <lib/toolbox/manchester_decoder.h>
#include <lib/toolbox/manchester_encoder.h>

#define FORD_V3_TE_SHORT          200U
#define FORD_V3_TE_LONG           400U
#define FORD_V3_TE_DELTA          260U
#define FORD_V3_INTER_BURST_GAP_US 15000U
#define FORD_V3_PREAMBLE_MIN      64U

#define FORD_V3_DATA_BYTES        17U
#define FORD_V3_DATA_BITS         136U
#define FORD_V3_SYNC_0            0x7FU
#define FORD_V3_SYNC_1            0xA7U
#define FORD_V3_CRC_LEN           12U
#define FORD_V3_CRC_OFFSET        3U
#define FORD_V3_CRC_POLY          0x1021U
#define FORD_V3_CRC_INIT          0x0000U
#define FORD_V3_CRYPT_OFFSET      7U
#define FORD_V3_CRYPT_LEN         8U

#define FORD_V3_ENC_TE_SHORT           240U
#define FORD_V3_ENC_PREAMBLE_PAIRS     70U
#define FORD_V3_ENC_BURST_COUNT        6U
#define FORD_V3_ENC_INTER_BURST_GAP_US 16000U
#define FORD_V3_ENC_ALLOC_ELEMS        2600U
#define FORD_V3_ENC_SYNC_LO_US         476U
#define FORD_V3_ENC_SEPARATOR_ELEMS    2U
#define FORD_V3_ENC_PREAMBLE_ELEMS     (FORD_V3_ENC_PREAMBLE_PAIRS * 2U)
#define FORD_V3_ENC_DATA_ELEMS         ((FORD_V3_DATA_BITS - 1U) * 2U)
#define FORD_V3_ENC_BURST_ELEMS \
    (FORD_V3_ENC_PREAMBLE_ELEMS + FORD_V3_ENC_SEPARATOR_ELEMS + FORD_V3_ENC_DATA_ELEMS)
#define FORD_V3_ENC_UPLOAD_ELEMS \
    (FORD_V3_ENC_BURST_COUNT * FORD_V3_ENC_BURST_ELEMS + (FORD_V3_ENC_BURST_COUNT - 1U))
#define FORD_V3_ENCODER_DEFAULT_REPEAT 10U

#define FORD_V3_SYNC_BITS                 16U
#define FORD_V3_POST_SYNC_DECODE_COUNT_BIT 16U
#define FORD_V3_PREAMBLE_COUNT_MAX        0xFFFFU

static const uint16_t ford_v3_sync_shift16_inv =
    (uint16_t)(~(((uint16_t)FORD_V3_SYNC_0 << 8) | (uint16_t)FORD_V3_SYNC_1));

static const SubGhzBlockConst subghz_protocol_ford_v3_const = {
    .te_short             = FORD_V3_TE_SHORT,
    .te_long              = FORD_V3_TE_LONG,
    .te_delta             = FORD_V3_TE_DELTA,
    .min_count_bit_for_found = FORD_V3_DATA_BITS,
};

typedef enum {
    FordV3DecoderStepReset    = 0,
    FordV3DecoderStepPreamble = 1,
    FordV3DecoderStepSync     = 2,
    FordV3DecoderStepData     = 3,
} FordV3DecoderStep;

typedef struct SubGhzProtocolDecoderFordV3 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder        decoder;
    SubGhzBlockGeneric        generic;

    ManchesterState manchester_state;
    uint16_t        preamble_count;

    uint8_t  raw_bytes[FORD_V3_DATA_BYTES];
    uint8_t  byte_count;

    uint16_t sync_shift;
    uint8_t  sync_bit_count;

    uint32_t serial;
    uint8_t  btn;
    uint16_t counter16;
    uint16_t crc_received;
    uint16_t crc_computed;
    bool     crc_ok;
    bool     structure_ok;

    uint8_t  crypt_buf[FORD_V3_CRYPT_LEN];
} SubGhzProtocolDecoderFordV3;

typedef struct SubGhzProtocolEncoderFordV3 {
    SubGhzProtocolEncoderBase  base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric         generic;

    uint8_t raw_bytes[FORD_V3_DATA_BYTES];

    uint8_t raw_freq0[FORD_V3_DATA_BYTES];
    uint8_t raw_freq1[FORD_V3_DATA_BYTES];
    uint8_t raw_freq2[FORD_V3_DATA_BYTES];
} SubGhzProtocolEncoderFordV3;

static void ford_v3_decoder_manchester_feed_event(
    SubGhzProtocolDecoderFordV3* instance,
    ManchesterEvent event);

static uint16_t ford_v3_crc16(const uint8_t* data, uint8_t len) {
    uint16_t crc = FORD_V3_CRC_INIT;
    while(len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for(uint8_t i = 0; i < 8U; i++) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ FORD_V3_CRC_POLY)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void ford_v3_crc_process(uint8_t* buf) {
    uint16_t crc = ford_v3_crc16(&buf[FORD_V3_CRC_OFFSET], FORD_V3_CRC_LEN);
    buf[15] = (uint8_t)((crc >> 8) & 0xFFU);
    buf[16] = (uint8_t)(crc & 0xFFU);
}

static uint8_t ford_v3_uint8_parity(uint8_t value) {
    uint8_t p = 0U;
    while(value) {
        p ^= (value & 1U);
        value >>= 1U;
    }
    return p;
}

static void ford_v3_encrypt_buffer_process(uint8_t* crypt) {
    uint8_t t0 = (0xAAU & crypt[5]) | (0x55U & crypt[6]);
    uint8_t t1 = (0x55U & crypt[5]) | (0xAAU & crypt[6]);
    crypt[5] = t0;
    crypt[6] = t1;

    uint8_t par = ford_v3_uint8_parity(crypt[7]);

    if(par) {
        uint8_t mask = crypt[6];
        crypt[0] ^= mask;
        crypt[1] ^= mask;
        crypt[2] ^= mask;
        crypt[3] ^= mask;
        crypt[4] ^= mask;
        crypt[5] ^= mask;
    } else {
        uint8_t mask = crypt[5];
        crypt[0] ^= mask;
        crypt[1] ^= mask;
        crypt[2] ^= mask;
        crypt[3] ^= mask;
        crypt[4] ^= mask;
        crypt[6] ^= mask;
    }
}

static void ford_v3_encrypt(uint8_t* buf) {
    uint8_t crypt[FORD_V3_CRYPT_LEN];
    for(uint8_t i = 0; i < FORD_V3_CRYPT_LEN; i++) {
        crypt[i] = buf[FORD_V3_CRYPT_OFFSET + i];
    }

    uint8_t sum = 0U;
    for(uint8_t i = 0; i < 7U; i++) sum += crypt[i];
    crypt[7] = sum;

    for(uint8_t i = 0; i < FORD_V3_CRYPT_LEN; i++) {
        buf[FORD_V3_CRYPT_OFFSET + i] = crypt[i];
    }

    ford_v3_encrypt_buffer_process(crypt);

    for(uint8_t i = 0; i < FORD_V3_CRYPT_LEN; i++) {
        buf[FORD_V3_CRYPT_OFFSET + i] = crypt[i];
    }
}

static void ford_v3_decrypt(const uint8_t* enc_block, uint8_t* crypt_out) {
    uint8_t crypt[20] = {0};
    for(uint8_t i = 0; i < FORD_V3_CRYPT_LEN; i++) crypt[i] = enc_block[i];

    crypt[17] = crypt[1];
    crypt[18] = 0x00U;
    {
        uint8_t tmp = crypt[17];
        while(tmp) {
            if(tmp & 1U) crypt[18] ^= 1U;
            tmp >>= 1U;
        }
    }

    if(crypt[18] & 0xFFU) {
        crypt[17] = enc_block[6];
        for(uint8_t i = 1; i < 7U; i++) {
            crypt[i] = (crypt[i] ^ crypt[17]) & 0xFFU;
        }
    } else {
        crypt[17] = enc_block[5];
        for(uint8_t i = 1; i < 6U; i++) {
            crypt[i] = (crypt[i] ^ crypt[17]) & 0xFFU;
        }
        crypt[7] ^= crypt[17];
    }

    crypt[19] = (crypt[6] & 0xAAU) | (crypt[7] & 0x55U);
    crypt[7]  = (crypt[7] & 0xAAU) | (crypt[6] & 0x55U);
    crypt[6]  = crypt[19] & 0xFFU;

    crypt[20 - 1] = 7U;
    crypt[17] = 0x00U;
    while(crypt[19]) {
        break;
    }
    {
        uint8_t cnt = 7U;
        uint8_t sum = 0U;
        while(cnt) {
            --cnt;
            sum += crypt[cnt];
        }
        crypt[17] = sum;
    }

    for(uint8_t i = 0; i < FORD_V3_CRYPT_LEN; i++) crypt_out[i] = crypt[i];
}

static bool ford_v3_button_is_valid(uint8_t btn) {
    switch(btn) {
    case 0x10:
    case 0x20:
    case 0x40:
        return true;
    default:
        return false;
    }
}

static const char* ford_v3_button_name(uint8_t btn) {
    switch(btn) {
    case 0x10: return "Lock";
    case 0x20: return "Unlock";
    case 0x40: return "Trunk";
    default:   return "Unknown";
    }
}

static void ford_v3_decoder_reset_state(SubGhzProtocolDecoderFordV3* instance) {
    instance->decoder.parser_step      = FordV3DecoderStepReset;
    instance->decoder.decode_data      = 0;
    instance->decoder.decode_count_bit = 0;
    instance->decoder.te_last          = 0;

    instance->byte_count    = 0;
    instance->sync_shift    = 0;
    instance->sync_bit_count = 0;
    instance->preamble_count = 0;
    instance->counter16     = 0;
    instance->crc_ok        = false;
    instance->structure_ok  = false;

    memset(instance->raw_bytes,  0, sizeof(instance->raw_bytes));
    memset(instance->crypt_buf,  0, sizeof(instance->crypt_buf));

    manchester_advance(
        instance->manchester_state, ManchesterEventReset,
        &instance->manchester_state, NULL);
}

static bool ford_v3_duration_is_short(uint32_t duration) {
    return DURATION_DIFF(duration, FORD_V3_TE_SHORT) < (int32_t)FORD_V3_TE_DELTA;
}

static bool ford_v3_duration_is_long(uint32_t duration) {
    return DURATION_DIFF(duration, FORD_V3_TE_LONG) < (int32_t)FORD_V3_TE_DELTA;
}

static void ford_v3_decoder_extract_from_raw(SubGhzProtocolDecoderFordV3* instance) {
    const uint8_t* k = instance->raw_bytes;

    instance->structure_ok = false;

    if(k[0] != FORD_V3_SYNC_0 || k[1] != FORD_V3_SYNC_1) return;

    instance->serial =
        ((uint32_t)k[4] << 16) | ((uint32_t)k[5] << 8) | (uint32_t)k[6];
    instance->generic.serial = instance->serial;

    ford_v3_decrypt(&k[FORD_V3_CRYPT_OFFSET], instance->crypt_buf);

    instance->btn         = instance->crypt_buf[4];
    instance->generic.btn = instance->btn;

    instance->counter16      = ((uint16_t)instance->crypt_buf[5] << 8) |
                               (uint16_t)instance->crypt_buf[6];
    instance->generic.cnt    = instance->counter16;

    instance->crc_received =
        ((uint16_t)k[15] << 8) | (uint16_t)k[16];
    instance->crc_computed = ford_v3_crc16(&k[FORD_V3_CRC_OFFSET], FORD_V3_CRC_LEN);
    instance->crc_ok = (instance->crc_received == instance->crc_computed);

    if(!instance->crc_ok) return;
    if(!ford_v3_button_is_valid(instance->btn)) return;

    instance->generic.data = 0;
    for(uint8_t i = 0; i < 8U; i++) {
        instance->generic.data = (instance->generic.data << 8) | (uint64_t)k[i];
    }
    instance->generic.data_count_bit = FORD_V3_DATA_BITS;

    instance->structure_ok = true;
}

static bool ford_v3_decoder_commit_frame(SubGhzProtocolDecoderFordV3* instance) {
    if(instance->raw_bytes[0] != FORD_V3_SYNC_0 ||
       instance->raw_bytes[1] != FORD_V3_SYNC_1) {
        return false;
    }

    ford_v3_decoder_extract_from_raw(instance);

    if(!instance->structure_ok) return false;

    if(instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
    return true;
}

static void ford_v3_decoder_sync_enter_data(SubGhzProtocolDecoderFordV3* instance) {
    memset(instance->raw_bytes, 0, sizeof(instance->raw_bytes));
    instance->raw_bytes[0] = FORD_V3_SYNC_0;
    instance->raw_bytes[1] = FORD_V3_SYNC_1;
    instance->byte_count               = 2U;
    instance->decoder.parser_step      = FordV3DecoderStepData;
    instance->decoder.decode_data      = 0;
    instance->decoder.decode_count_bit = FORD_V3_POST_SYNC_DECODE_COUNT_BIT;
}

static bool ford_v3_decoder_sync_feed_event(
    SubGhzProtocolDecoderFordV3* instance,
    ManchesterEvent event) {
    bool data_bit;

    if(!manchester_advance(
           instance->manchester_state, event,
           &instance->manchester_state, &data_bit)) {
        return false;
    }

    instance->sync_shift =
        (uint16_t)((instance->sync_shift << 1) | (data_bit ? 1U : 0U));
    if(instance->sync_bit_count < FORD_V3_SYNC_BITS) {
        instance->sync_bit_count++;
    }

    return (instance->sync_bit_count >= FORD_V3_SYNC_BITS) &&
           (instance->sync_shift == ford_v3_sync_shift16_inv);
}

static void ford_v3_decoder_manchester_feed_event(
    SubGhzProtocolDecoderFordV3* instance,
    ManchesterEvent event) {
    bool data_bit;

    if(instance->decoder.parser_step == FordV3DecoderStepSync) {
        if(ford_v3_decoder_sync_feed_event(instance, event)) {
            ford_v3_decoder_sync_enter_data(instance);
        }
        return;
    }

    if(!manchester_advance(
           instance->manchester_state, event,
           &instance->manchester_state, &data_bit)) {
        return;
    }

    if(instance->decoder.parser_step != FordV3DecoderStepData) return;

    data_bit = !data_bit;

    instance->decoder.decode_data =
        (instance->decoder.decode_data << 1) | (data_bit ? 1U : 0U);
    instance->decoder.decode_count_bit++;

    if((instance->decoder.decode_count_bit & 7U) == 0U) {
        uint8_t byte_val = (uint8_t)(instance->decoder.decode_data & 0xFFU);

        if(instance->byte_count < FORD_V3_DATA_BYTES) {
            instance->raw_bytes[instance->byte_count] = byte_val;
            instance->byte_count++;
        }

        instance->decoder.decode_data = 0;

        if(instance->byte_count == FORD_V3_DATA_BYTES) {
            (void)ford_v3_decoder_commit_frame(instance);
            ford_v3_decoder_reset_state(instance);
        }
    }
}

static bool ford_v3_decoder_manchester_feed_pulse(
    SubGhzProtocolDecoderFordV3* instance,
    bool level,
    uint32_t duration) {
    if(ford_v3_duration_is_short(duration)) {
        ManchesterEvent ev =
            level ? ManchesterEventShortHigh : ManchesterEventShortLow;
        ford_v3_decoder_manchester_feed_event(instance, ev);
        return true;
    }
    if(ford_v3_duration_is_long(duration)) {
        ManchesterEvent ev =
            level ? ManchesterEventLongHigh : ManchesterEventLongLow;
        ford_v3_decoder_manchester_feed_event(instance, ev);
        return true;
    }
    return false;
}

static void ford_v3_decoder_enter_sync_from_preamble(
    SubGhzProtocolDecoderFordV3* instance,
    bool level,
    uint32_t duration) {
    instance->decoder.parser_step      = FordV3DecoderStepSync;
    instance->decoder.decode_data      = 0;
    instance->decoder.decode_count_bit = 0;
    instance->byte_count               = 0;
    instance->sync_shift               = 0;
    instance->sync_bit_count           = 0;
    memset(instance->raw_bytes, 0, sizeof(instance->raw_bytes));

    manchester_advance(
        instance->manchester_state, ManchesterEventReset,
        &instance->manchester_state, NULL);

    if(ford_v3_duration_is_short(duration)) {
        ManchesterEvent ev =
            level ? ManchesterEventShortHigh : ManchesterEventShortLow;
        ford_v3_decoder_manchester_feed_event(instance, ev);
    } else if(ford_v3_duration_is_long(duration)) {
        ManchesterEvent ev =
            level ? ManchesterEventLongHigh : ManchesterEventLongLow;
        ford_v3_decoder_manchester_feed_event(instance, ev);
    } else {
        ford_v3_decoder_reset_state(instance);
    }
}

static inline void ford_v3_encoder_add_level(
    SubGhzProtocolEncoderFordV3* instance,
    bool level,
    uint32_t duration) {
    size_t idx = instance->encoder.size_upload;
    if(idx > 0 &&
       level_duration_get_level(instance->encoder.upload[idx - 1]) == level) {
        uint32_t prev =
            level_duration_get_duration(instance->encoder.upload[idx - 1]);
        instance->encoder.upload[idx - 1] =
            level_duration_make(level, prev + duration);
    } else {
        furi_check(idx < FORD_V3_ENC_ALLOC_ELEMS);
        instance->encoder.upload[idx] = level_duration_make(level, duration);
        instance->encoder.size_upload++;
    }
}

static inline void ford_v3_encoder_emit_manchester_bit(
    SubGhzProtocolEncoderFordV3* instance,
    bool bit) {
    if(bit) {
        ford_v3_encoder_add_level(instance, true,  FORD_V3_ENC_TE_SHORT);
        ford_v3_encoder_add_level(instance, false, FORD_V3_ENC_TE_SHORT);
    } else {
        ford_v3_encoder_add_level(instance, false, FORD_V3_ENC_TE_SHORT);
        ford_v3_encoder_add_level(instance, true,  FORD_V3_ENC_TE_SHORT);
    }
}

static void ford_v3_encoder_emit_burst(
    SubGhzProtocolEncoderFordV3* instance,
    const uint8_t* raw) {
    for(uint8_t i = 0; i < FORD_V3_ENC_PREAMBLE_PAIRS; i++) {
        ford_v3_encoder_add_level(instance, false, FORD_V3_ENC_TE_SHORT);
        ford_v3_encoder_add_level(instance, true,  FORD_V3_ENC_TE_SHORT);
    }

    ford_v3_encoder_add_level(instance, false, FORD_V3_ENC_SYNC_LO_US);
    ford_v3_encoder_add_level(instance, true,  FORD_V3_ENC_TE_SHORT);

    for(uint16_t bit_pos = 1U; bit_pos < FORD_V3_DATA_BITS; bit_pos++) {
        const uint8_t byte_idx = (uint8_t)(bit_pos / 8U);
        const uint8_t bit_idx  = (uint8_t)(7U - (bit_pos % 8U));
        ford_v3_encoder_emit_manchester_bit(
            instance,
            ((raw[byte_idx] >> bit_idx) & 1U) != 0U);
    }
}

/**
 * Ford V3 transmits three frequency variants (freq-id 0x00, 0x08, 0x10)
 * per burst cycle, each followed by inter-burst gap, cycling
 * FORD_V3_ENC_BURST_COUNT times total.
 *
 * For simplicity on Flipper (single-frequency TX) we encode only the
 * freq-id 0x00 variant repeated BURST_COUNT times, matching the V2 pattern.
 * Can replay the other variants by editing the .sub file.
 */
static void ford_v3_encoder_build_upload(SubGhzProtocolEncoderFordV3* instance) {
    instance->encoder.size_upload = 0;
    instance->encoder.front       = 0;

    for(uint8_t burst = 0; burst < FORD_V3_ENC_BURST_COUNT; burst++) {
        ford_v3_encoder_emit_burst(instance, instance->raw_bytes);

        if(burst + 1U < FORD_V3_ENC_BURST_COUNT) {
            ford_v3_encoder_add_level(
                instance, true, FORD_V3_ENC_INTER_BURST_GAP_US);
        }
    }
}

static void ford_v3_encoder_rebuild_raw_from_payload(
    SubGhzProtocolEncoderFordV3* instance) {
    instance->raw_bytes[0] = FORD_V3_SYNC_0;
    instance->raw_bytes[1] = FORD_V3_SYNC_1;
    instance->raw_bytes[2] = 0x00U;
    /* freq-id – use default (0x00 = 433.6 MHz channel) */
    instance->raw_bytes[3] = 0x00U;

    instance->raw_bytes[4] = (uint8_t)((instance->generic.serial >> 16) & 0xFFU);
    instance->raw_bytes[5] = (uint8_t)((instance->generic.serial >>  8) & 0xFFU);
    instance->raw_bytes[6] = (uint8_t)( instance->generic.serial        & 0xFFU);

    uint8_t crypt[FORD_V3_CRYPT_LEN] = {0};
    crypt[4] = instance->generic.btn;
    crypt[5] = (uint8_t)((instance->generic.cnt >> 8) & 0xFFU);
    crypt[6] = (uint8_t)( instance->generic.cnt       & 0xFFU);

    for(uint8_t i = 0; i < FORD_V3_CRYPT_LEN; i++) {
        instance->raw_bytes[FORD_V3_CRYPT_OFFSET + i] = crypt[i];
    }

    ford_v3_encrypt(instance->raw_bytes);
    ford_v3_crc_process(instance->raw_bytes);
}

void* subghz_protocol_encoder_ford_v3_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderFordV3* instance =
        calloc(1, sizeof(SubGhzProtocolEncoderFordV3));
    furi_check(instance);

    instance->base.protocol         = &ford_protocol_v3;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat        = FORD_V3_ENCODER_DEFAULT_REPEAT;
    instance->encoder.upload =
        calloc(FORD_V3_ENC_ALLOC_ELEMS, sizeof(LevelDuration));
    furi_check(instance->encoder.upload);

    return instance;
}

void subghz_protocol_encoder_ford_v3_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFordV3* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

SubGhzProtocolStatus subghz_protocol_encoder_ford_v3_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderFordV3* instance = context;

    instance->encoder.is_running        = false;
    instance->encoder.front             = 0;
    instance->encoder.repeat            = FORD_V3_ENCODER_DEFAULT_REPEAT;
    instance->generic.data_count_bit    = FORD_V3_DATA_BITS;

    FuriString* temp_str = furi_string_alloc();
    furi_check(temp_str);

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) break;
        if(!furi_string_equal(temp_str, instance->base.protocol->name)) break;
        SubGhzProtocolStatus g = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic, flipper_format, FORD_V3_DATA_BITS);
        if(g != SubGhzProtocolStatusOk) {
            ret = g;
            break;
        }

        flipper_format_rewind(flipper_format);
        uint8_t raw_tmp[FORD_V3_DATA_BYTES] = {0};
        if(flipper_format_read_hex(
               flipper_format, "RawBytes", raw_tmp, sizeof(raw_tmp))) {
            memcpy(instance->raw_bytes, raw_tmp, sizeof(raw_tmp));
        } else {
            ford_v3_encoder_rebuild_raw_from_payload(instance);
        }

        if(!ford_v3_button_is_valid(instance->raw_bytes[FORD_V3_CRYPT_OFFSET + 4])) {
            if(!ford_v3_button_is_valid(instance->generic.btn)) {
                ret = SubGhzProtocolStatusErrorParserOthers;
                break;
            }
        }

        flipper_format_rewind(flipper_format);
        uint32_t repeat = FORD_V3_ENCODER_DEFAULT_REPEAT;
        if(flipper_format_read_uint32(flipper_format, "Repeat", &repeat, 1)) {
            instance->encoder.repeat = repeat;
        }

        ford_v3_encoder_build_upload(instance);
        instance->encoder.is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    furi_string_free(temp_str);
    return ret;
}

void subghz_protocol_encoder_ford_v3_stop(void* context) {
    furi_check(context);
    ((SubGhzProtocolEncoderFordV3*)context)->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_ford_v3_yield(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFordV3* instance = context;

    if(!instance->encoder.is_running || instance->encoder.repeat == 0U) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.front = 0U;
        instance->encoder.repeat--;
    }

    return ret;
}

void* subghz_protocol_decoder_ford_v3_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderFordV3* instance =
        calloc(1, sizeof(SubGhzProtocolDecoderFordV3));
    furi_check(instance);

    instance->base.protocol         = &ford_protocol_v3;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void subghz_protocol_decoder_ford_v3_free(void* context) {
    furi_check(context);
    free(context);
}

void subghz_protocol_decoder_ford_v3_reset(void* context) {
    furi_check(context);
    ford_v3_decoder_reset_state((SubGhzProtocolDecoderFordV3*)context);
}

void subghz_protocol_decoder_ford_v3_feed(
    void* context,
    bool level,
    uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderFordV3* instance = context;

    switch(instance->decoder.parser_step) {
    case FordV3DecoderStepReset:
        if(ford_v3_duration_is_short(duration)) {
            instance->preamble_count     = 1U;
            instance->decoder.parser_step = FordV3DecoderStepPreamble;
        }
        break;

    case FordV3DecoderStepPreamble:
        if(ford_v3_duration_is_short(duration)) {
            if(instance->preamble_count < FORD_V3_PREAMBLE_COUNT_MAX) {
                instance->preamble_count++;
            }
        } else if(!level && ford_v3_duration_is_long(duration)) {
            if(instance->preamble_count >= FORD_V3_PREAMBLE_MIN) {
                ford_v3_decoder_enter_sync_from_preamble(instance, level, duration);
            } else {
                ford_v3_decoder_reset_state(instance);
            }
        } else {
            ford_v3_decoder_reset_state(instance);
        }
        break;

    case FordV3DecoderStepSync:
    case FordV3DecoderStepData:
        if(!ford_v3_decoder_manchester_feed_pulse(instance, level, duration)) {
            if(duration >= FORD_V3_INTER_BURST_GAP_US) {
                ford_v3_decoder_reset_state(instance);
            } else {
                ford_v3_decoder_reset_state(instance);
            }
        }
        instance->decoder.te_last = duration;
        break;
    }
}

uint8_t subghz_protocol_decoder_ford_v3_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFordV3* instance = context;
    const uint8_t* k = instance->raw_bytes;

    uint32_t mix =
        ((uint32_t)k[4] << 16) | ((uint32_t)k[5] << 8) | (uint32_t)k[6];
    mix ^= (uint32_t)instance->btn    << 16;
    mix ^= (uint32_t)instance->counter16 << 8;
    mix ^= ((uint16_t)k[15] << 8) | k[16];

    return (uint8_t)(
        (mix >> 0) ^ (mix >> 8) ^ (mix >> 16) ^ (mix >> 24) ^
        (uint8_t)(instance->counter16 >> 8) ^
        (uint8_t)(instance->crc_received >> 8));
}

SubGhzProtocolStatus subghz_protocol_decoder_ford_v3_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderFordV3* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if(ret == SubGhzProtocolStatusOk) {
        uint32_t serial = instance->generic.serial;
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_uint32(
            flipper_format, "Serial", &serial, 1);

        uint32_t btn = instance->generic.btn;
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_uint32(
            flipper_format, "Btn", &btn, 1);

        uint32_t cnt = instance->generic.cnt;
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_uint32(
            flipper_format, "Cnt", &cnt, 1);

        uint32_t crc = instance->crc_received;
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_uint32(
            flipper_format, "CRC", &crc, 1);

        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_hex(
            flipper_format, "RawBytes",
            instance->raw_bytes, FORD_V3_DATA_BYTES);
    }

    return ret;
}

SubGhzProtocolStatus subghz_protocol_decoder_ford_v3_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFordV3* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            subghz_protocol_ford_v3_const.min_count_bit_for_found);

    if(ret != SubGhzProtocolStatusOk) return ret;

    if(instance->generic.data_count_bit != FORD_V3_DATA_BITS) {
        return SubGhzProtocolStatusErrorValueBitCount;
    }

    flipper_format_rewind(flipper_format);
    uint8_t raw_tmp[FORD_V3_DATA_BYTES] = {0};
    if(flipper_format_read_hex(
           flipper_format, "RawBytes", raw_tmp, sizeof(raw_tmp))) {
        memcpy(instance->raw_bytes, raw_tmp, sizeof(raw_tmp));
    } else {
        instance->raw_bytes[0] = FORD_V3_SYNC_0;
        instance->raw_bytes[1] = FORD_V3_SYNC_1;
    }

    ford_v3_decoder_extract_from_raw(instance);

    if(!instance->structure_ok) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    return SubGhzProtocolStatusOk;
}

void subghz_protocol_decoder_ford_v3_get_string(
    void* context,
    FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderFordV3* instance = context;
    const uint8_t* k = instance->raw_bytes;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Raw:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n"
        "Sn:%06lX Btn:%02X [%s]\r\n"
        "Cnt:%u CRC:%s(%04X)\r\n"
        "Struct:%s\r\n",
        instance->generic.protocol_name,
        (int)instance->generic.data_count_bit,
        k[0],  k[1],  k[2],  k[3],  k[4],  k[5],  k[6],  k[7],
        k[8],  k[9],  k[10], k[11], k[12], k[13], k[14], k[15], k[16],
        (unsigned long)instance->generic.serial,
        instance->generic.btn,
        ford_v3_button_name(instance->generic.btn),
        (unsigned)instance->counter16,
        instance->crc_ok ? "OK" : "BAD",
        (unsigned)instance->crc_received,
        instance->structure_ok ? "OK" : "BAD");
}

const SubGhzProtocolDecoder subghz_protocol_ford_v3_decoder = {
    .alloc         = subghz_protocol_decoder_ford_v3_alloc,
    .free          = subghz_protocol_decoder_ford_v3_free,
    .feed          = subghz_protocol_decoder_ford_v3_feed,
    .reset         = subghz_protocol_decoder_ford_v3_reset,
    .get_hash_data = subghz_protocol_decoder_ford_v3_get_hash_data,
    .serialize     = subghz_protocol_decoder_ford_v3_serialize,
    .deserialize   = subghz_protocol_decoder_ford_v3_deserialize,
    .get_string    = subghz_protocol_decoder_ford_v3_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_ford_v3_encoder = {
    .alloc       = subghz_protocol_encoder_ford_v3_alloc,
    .free        = subghz_protocol_encoder_ford_v3_free,
    .deserialize = subghz_protocol_encoder_ford_v3_deserialize,
    .stop        = subghz_protocol_encoder_ford_v3_stop,
    .yield       = subghz_protocol_encoder_ford_v3_yield,
};

const SubGhzProtocol ford_protocol_v3 = {
    .name    = FORD_PROTOCOL_V3_NAME,
    .type    = SubGhzProtocolTypeDynamic,
    .flag    = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM |
               SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
               SubGhzProtocolFlag_Save      | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_ford_v3_decoder,
    .encoder = &subghz_protocol_ford_v3_encoder,
};
