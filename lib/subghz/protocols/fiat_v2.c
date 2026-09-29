#include "fiat_v2.h"
#include "fiat_v1.h"
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>

#include <lib/subghz/blocks/custom_btn_i.h>
#include <string.h>

#define TAG "FiatProtocolV2"

#define FIAT_V2_TE_SHORT          210U
#define FIAT_V2_TE_LONG           420U
#define FIAT_V2_TE_DELTA          100U
#define FIAT_V2_BOUNDARY_MIN_US   900U
#define FIAT_V2_WIRE_BITS         112U
#define FIAT_V2_WIRE_BYTES        14U
#define FIAT_V2_WIRE_CELLS        (FIAT_V2_WIRE_BITS * 2U)
#define FIAT_V2_LOGICAL_BITS      112U
#define FIAT_V2_MARKER0           0x00U
#define FIAT_V2_MARKER1           0x01U
#define FIAT_V2_BTN_SHIFT         6U
#define FIAT_V2_BUTTON_LOCK       0x2U
#define FIAT_V2_BUTTON_UNLOCK     0x3U
#define FIAT_V2_BUTTON_TRUNK      0x1U
#define FIAT_V2_CNT_SHIFT         3U
#define FIAT_V2_FCA_TYPE_NIBBLE   0xD0U
#define FIAT_V2_RAW_FIELD         "Raw"
#define FIAT_V2_HOP_FIELD         "Hop"
#define FIAT_V2_BTN_FIELD         "Btn"
#define FIAT_V2_HITAG2_KEY_FIELD   "Hitag2 Key"
#define FIAT_V2_HITAG2_EPOCH_FIELD "Hitag2 Epoch"
#define FIAT_V2_HITAG2_IV_FIELD    "Hitag2 IV"

#define FIAT_V2_ENC_LEAD_US        2033U
#define FIAT_V2_ENC_GAP_US         3252U
#define FIAT_V2_ENC_DEFAULT_REPEAT 6U
#define FIAT_V2_UPLOAD_CAPACITY   (1U + (FIAT_V2_WIRE_BITS * 2U) + 1U)

static uint32_t fiat_v2_uid(const uint8_t raw[FIAT_V2_WIRE_BYTES]);
static uint32_t fiat_v2_hop(const uint8_t raw[FIAT_V2_WIRE_BYTES]);
static uint32_t fiat_v2_counter(const uint8_t raw[FIAT_V2_WIRE_BYTES]);
static uint8_t fiat_v2_iv_button(const uint8_t raw[FIAT_V2_WIRE_BYTES], uint8_t combo);
static uint16_t fiat_v2_iv_control(const uint8_t raw[FIAT_V2_WIRE_BYTES], uint8_t combo);
static bool fiat_v2_key_matches_combo(
    const uint8_t raw[FIAT_V2_WIRE_BYTES],
    const uint8_t key[6],
    uint8_t combo);
static void fiat_v2_patch_hop(uint8_t raw[FIAT_V2_WIRE_BYTES], uint32_t hop);
static void fiat_v2_patch_button(uint8_t raw[FIAT_V2_WIRE_BYTES], uint8_t selector);
static void fiat_v2_patch_counter(uint8_t raw[FIAT_V2_WIRE_BYTES], uint32_t counter);

static const SubGhzBlockConst subghz_protocol_fiat_v2_const = {
    .te_short = FIAT_V2_TE_SHORT,
    .te_long = FIAT_V2_TE_LONG,
    .te_delta = FIAT_V2_TE_DELTA,
    .min_count_bit_for_found = FIAT_V2_LOGICAL_BITS,
};

typedef enum {
    FiatV2DecoderStepReset = 0,
    FiatV2DecoderStepData = 1,
} FiatV2DecoderStep;

struct SubGhzProtocolDecoderFiatV2 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint8_t cells[FIAT_V2_WIRE_CELLS];
    uint16_t cell_count;

    uint8_t raw_data[FIAT_V2_WIRE_BYTES];
    uint8_t last_raw_data[FIAT_V2_WIRE_BYTES];
    bool last_raw_valid;

    uint32_t uid;
    uint32_t hop;
    uint8_t button;

    uint8_t hitag2_key[6];
    uint32_t hitag2_epoch;
    bool hitag2_key_valid;
    uint8_t hitag2_iv_combo;
};

typedef struct SubGhzProtocolEncoderFiatV2 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint8_t raw_data[FIAT_V2_WIRE_BYTES];
    uint8_t hitag2_key[6];
    uint32_t epoch;
    uint8_t iv_combo;
} SubGhzProtocolEncoderFiatV2;

static bool fiat_v2_feed_data_pulse(
    SubGhzProtocolDecoderFiatV2* instance,
    bool level,
    uint32_t duration);
static bool fiat_v2_frame_valid(const uint8_t raw[FIAT_V2_WIRE_BYTES]);
static void fiat_v2_verify_hitag2_key(SubGhzProtocolDecoderFiatV2* instance);

static void subghz_protocol_decoder_fiat_v2_free(void* context) {
    furi_assert(context);
    free(context);
}

const SubGhzProtocolDecoder subghz_protocol_fiat_v2_decoder = {
    .alloc = subghz_protocol_decoder_fiat_v2_alloc,
    .free = subghz_protocol_decoder_fiat_v2_free,
    .feed = subghz_protocol_decoder_fiat_v2_feed,
    .reset = subghz_protocol_decoder_fiat_v2_reset,
    .get_hash_data = subghz_protocol_decoder_fiat_v2_get_hash_data,
    .serialize = subghz_protocol_decoder_fiat_v2_serialize,
    .deserialize = subghz_protocol_decoder_fiat_v2_deserialize,
    .get_string = subghz_protocol_decoder_fiat_v2_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_fiat_v2_encoder = {
    .alloc = subghz_protocol_encoder_fiat_v2_alloc,
    .free = subghz_protocol_encoder_fiat_v2_free,
    .deserialize = subghz_protocol_encoder_fiat_v2_deserialize,
    .stop = subghz_protocol_encoder_fiat_v2_stop,
    .yield = subghz_protocol_encoder_fiat_v2_yield,
};

const SubGhzProtocol fiat_v2_protocol = {
    .name = FIAT_V2_PROTOCOL_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_fiat_v2_decoder,
    .encoder = &subghz_protocol_fiat_v2_encoder,
};

void* subghz_protocol_encoder_fiat_v2_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderFiatV2* instance = calloc(1, sizeof(SubGhzProtocolEncoderFiatV2));
    furi_check(instance);

    instance->base.protocol = &fiat_v2_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat = FIAT_V2_ENC_DEFAULT_REPEAT;
    instance->encoder.upload = malloc(FIAT_V2_UPLOAD_CAPACITY * sizeof(LevelDuration));
    furi_check(instance->encoder.upload);

    return instance;
}

void subghz_protocol_encoder_fiat_v2_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderFiatV2* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

void subghz_protocol_encoder_fiat_v2_stop(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderFiatV2* instance = context;
    instance->encoder.is_running = false;
    instance->encoder.front = 0;
}

LevelDuration subghz_protocol_encoder_fiat_v2_yield(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderFiatV2* instance = context;

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

static bool fiat_v2_encoder_build_upload(SubGhzProtocolEncoderFiatV2* instance) {
    furi_check(instance);
    LevelDuration* upload = instance->encoder.upload;
    if(!upload) {
        return false;
    }

    size_t index = 0U;

    upload[index++] = level_duration_make(true, FIAT_V2_ENC_LEAD_US);

    for(uint8_t bit_index = 0U; bit_index < FIAT_V2_WIRE_BITS; bit_index++) {
        const bool bit =
            ((instance->raw_data[bit_index >> 3U] >> (7U - (bit_index & 7U))) & 1U) != 0U;
        upload[index++] = level_duration_make(bit, FIAT_V2_TE_SHORT);
        upload[index++] = level_duration_make(!bit, FIAT_V2_TE_SHORT);
    }

    upload[index++] = level_duration_make(false, FIAT_V2_ENC_GAP_US);
    instance->encoder.size_upload = index;
    instance->encoder.front = 0U;
    return true;
}

static uint8_t fiat_v2_dpad_selector(uint8_t custom_btn_id, uint8_t original_selector) {
    switch(custom_btn_id) {
    case SUBGHZ_CUSTOM_BTN_UP:
        return FIAT_V2_BUTTON_UNLOCK;
    case SUBGHZ_CUSTOM_BTN_DOWN:
        return FIAT_V2_BUTTON_LOCK;
    case SUBGHZ_CUSTOM_BTN_LEFT:
        return FIAT_V2_BUTTON_TRUNK;
    case SUBGHZ_CUSTOM_BTN_RIGHT:
    case SUBGHZ_CUSTOM_BTN_OK:
    default:
        return original_selector;
    }
}

SubGhzProtocolStatus
    subghz_protocol_encoder_fiat_v2_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    furi_check(flipper_format);
    SubGhzProtocolEncoderFiatV2* instance = context;

    instance->encoder.is_running = false;
    instance->encoder.front = 0U;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_fiat_v2_const.min_count_bit_for_found);
    if(ret != SubGhzProtocolStatusOk) {
        return ret;
    }
    if(instance->generic.data_count_bit != FIAT_V2_LOGICAL_BITS) {
        return SubGhzProtocolStatusErrorValueBitCount;
    }

    flipper_format_rewind(flipper_format);
    uint8_t raw_tmp[FIAT_V2_WIRE_BYTES] = {0};
    if(!flipper_format_read_hex(flipper_format, FIAT_V2_RAW_FIELD, raw_tmp, sizeof(raw_tmp)) ||
       !fiat_v2_frame_valid(raw_tmp)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    memcpy(instance->raw_data, raw_tmp, sizeof(raw_tmp));

    instance->generic.serial = fiat_v2_uid(instance->raw_data);
    instance->generic.btn = instance->raw_data[7];
    instance->generic.cnt = fiat_v2_counter(instance->raw_data);
    instance->epoch = 0U;

    uint32_t override_cnt = 0U;
    bool got_cnt = false;
    {
        uint32_t tmp = 0U;
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Serial", &tmp, 1)) {
            instance->generic.serial = tmp;
        }
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Cnt", &tmp, 1)) {
            override_cnt = tmp;
            got_cnt = true;
            instance->generic.cnt = tmp;
        }
    }

    const uint8_t original_selector =
        (uint8_t)((instance->raw_data[7] >> FIAT_V2_BTN_SHIFT) & 0x03U);
    if(subghz_custom_btn_get_original() == 0) {
        subghz_custom_btn_set_original(original_selector);
    }
    subghz_custom_btn_set_max(3);
    const uint8_t custom_btn_id = subghz_custom_btn_get();

    uint8_t key[6] = {0};
    bool have_key = false;
    uint8_t combo = 0U;

    flipper_format_rewind(flipper_format);
    if(flipper_format_read_hex(flipper_format, FIAT_V2_HITAG2_KEY_FIELD, key, 6U)) {
        uint32_t iv_combo = 0U;
        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, FIAT_V2_HITAG2_IV_FIELD, &iv_combo, 1U)) {
            iv_combo = 0U;
        }
        combo = (uint8_t)(iv_combo & 0x03U);
        if(fiat_v2_key_matches_combo(instance->raw_data, key, combo)) {
            have_key = true;
        }
    }

    if(!have_key) {
        const uint8_t(*known_keys)[6] = subghz_protocol_fiat_v1_get_known_keys();
        for(uint8_t i = 0U; !have_key && i < FIAT_V1_KNOWN_KEY_COUNT; i++) {
            for(uint8_t c = 0U; c < FIAT_V2_IV_COMBO_COUNT; c++) {
                if(fiat_v2_key_matches_combo(instance->raw_data, known_keys[i], c)) {
                    memcpy(key, known_keys[i], 6U);
                    combo = c;
                    have_key = true;
                    break;
                }
            }
        }
    }

    if(have_key) {
        memcpy(instance->hitag2_key, key, 6U);
        instance->iv_combo = combo;

        const uint8_t new_selector =
            fiat_v2_dpad_selector(custom_btn_id, original_selector);

        uint32_t new_counter;
        if(got_cnt) {
            new_counter = override_cnt;
        } else {
            uint32_t mult = furi_hal_subghz_get_rolling_counter_mult();
            if(mult == 0U) mult = 1U;
            new_counter = instance->generic.cnt + mult;
        }

        fiat_v2_patch_button(instance->raw_data, new_selector);
        fiat_v2_patch_counter(instance->raw_data, new_counter);

        instance->generic.btn = instance->raw_data[7];
        instance->generic.cnt = fiat_v2_counter(instance->raw_data);

        const uint32_t uid = fiat_v2_uid(instance->raw_data);
        const uint8_t iv_btn = fiat_v2_iv_button(instance->raw_data, combo);
        const uint16_t iv_ctrl = fiat_v2_iv_control(instance->raw_data, combo);
        const uint32_t hop =
            subghz_protocol_fiat_v1_compute_auth(uid, iv_btn, iv_ctrl, key, 0U);
        fiat_v2_patch_hop(instance->raw_data, hop);

        FURI_LOG_I(
            TAG,
            "TX(hop-recompute) UID:%08lX Combo:%u Sel:%u Cnt:%04lX Hop:%08lX",
            (unsigned long)uid,
            (unsigned)combo,
            (unsigned)new_selector,
            (unsigned long)instance->generic.cnt,
            (unsigned long)hop);
    } else {
        instance->generic.cnt = fiat_v2_counter(instance->raw_data);
        instance->generic.btn = instance->raw_data[7];
        FURI_LOG_I(
            TAG,
            "TX(replay) UID:%08lX (no key, replaying captured frame)",
            (unsigned long)instance->generic.serial);
    }

    instance->generic.data =
        ((uint64_t)instance->generic.serial << 32U) | fiat_v2_hop(instance->raw_data);

    if(have_key) {
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_hex(
            flipper_format, FIAT_V2_RAW_FIELD, instance->raw_data, FIAT_V2_WIRE_BYTES);
        flipper_format_rewind(flipper_format);
        uint32_t cnt_store = instance->generic.cnt;
        flipper_format_insert_or_update_uint32(flipper_format, "Cnt", &cnt_store, 1);
    }

    uint32_t repeat = FIAT_V2_ENC_DEFAULT_REPEAT;
    flipper_format_rewind(flipper_format);
    flipper_format_read_uint32(flipper_format, "Repeat", &repeat, 1);
    instance->encoder.repeat = (repeat == 0U) ? FIAT_V2_ENC_DEFAULT_REPEAT : (size_t)repeat;

    if(!fiat_v2_encoder_build_upload(instance)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    instance->encoder.is_running = true;

    return SubGhzProtocolStatusOk;
}

static bool fiat_v2_duration_is_short(uint32_t duration) {
    return DURATION_DIFF(duration, subghz_protocol_fiat_v2_const.te_short) <
           subghz_protocol_fiat_v2_const.te_delta;
}

static bool fiat_v2_duration_is_long(uint32_t duration) {
    return DURATION_DIFF(duration, subghz_protocol_fiat_v2_const.te_long) <
           subghz_protocol_fiat_v2_const.te_delta;
}

static bool fiat_v2_button_valid(uint8_t button) {
    const uint8_t sel = button >> FIAT_V2_BTN_SHIFT;
    return sel == FIAT_V2_BUTTON_LOCK || sel == FIAT_V2_BUTTON_UNLOCK ||
           sel == FIAT_V2_BUTTON_TRUNK;
}

static const char* fiat_v2_button_name(uint8_t button) {
    switch(button >> FIAT_V2_BTN_SHIFT) {
    case FIAT_V2_BUTTON_LOCK:
        return "Lock";
    case FIAT_V2_BUTTON_UNLOCK:
        return "Unlock";
    case FIAT_V2_BUTTON_TRUNK:
        return "Trunk";
    default:
        return "Unknown";
    }
}

static uint8_t fiat_v2_ui_selector(uint8_t custom_btn_id, uint8_t original_selector) {
    switch(custom_btn_id) {
    case SUBGHZ_CUSTOM_BTN_UP:
        return FIAT_V2_BUTTON_UNLOCK;
    case SUBGHZ_CUSTOM_BTN_DOWN:
        return FIAT_V2_BUTTON_LOCK;
    case SUBGHZ_CUSTOM_BTN_LEFT:
        return FIAT_V2_BUTTON_TRUNK;
    case SUBGHZ_CUSTOM_BTN_RIGHT:
    case SUBGHZ_CUSTOM_BTN_OK:
    default:
        return original_selector;
    }
}

static uint32_t fiat_v2_uid(const uint8_t raw[FIAT_V2_WIRE_BYTES]) {
    return ((uint32_t)raw[2] << 24U) | ((uint32_t)raw[3] << 16U) |
           ((uint32_t)raw[4] << 8U) | raw[5];
}

static bool fiat_v2_is_fca(const uint8_t raw[FIAT_V2_WIRE_BYTES]) {
    return (raw[6] & 0xF0U) == FIAT_V2_FCA_TYPE_NIBBLE;
}
static uint32_t fiat_v2_hop(const uint8_t raw[FIAT_V2_WIRE_BYTES]) {
    if(fiat_v2_is_fca(raw)) {
        return ((uint32_t)raw[10] << 24U) | ((uint32_t)raw[11] << 16U) |
               ((uint32_t)raw[12] << 8U) | raw[13];
    }
    return ((uint32_t)raw[9] << 24U) | ((uint32_t)raw[10] << 16U) |
           ((uint32_t)raw[11] << 8U) | raw[12];
}

static uint32_t fiat_v2_counter(const uint8_t raw[FIAT_V2_WIRE_BYTES]) {
    if(fiat_v2_is_fca(raw)) {
        const uint32_t raw_cnt = ((uint32_t)raw[8] << 6U) | (uint32_t)(raw[9] >> 2U);
        return (~raw_cnt) & 0x3FFFU;
    }
    const uint32_t raw_cnt =
        ((uint32_t)(raw[7] & 0x3FU) << 5U) | (uint32_t)(raw[8] >> FIAT_V2_CNT_SHIFT);
    return (~raw_cnt) & 0x7FFU;
}

static uint8_t fiat_v2_iv_button(const uint8_t raw[FIAT_V2_WIRE_BYTES], uint8_t combo) {
    const uint8_t sel = (uint8_t)((raw[7] >> FIAT_V2_BTN_SHIFT) & 0x0FU);
    if((combo & 0x01U) == 0U) {
        return sel;
    }
    switch(sel) {
    case FIAT_V2_BUTTON_TRUNK:
        return 0x2U;
    case FIAT_V2_BUTTON_LOCK:
        return 0x4U;
    case FIAT_V2_BUTTON_UNLOCK:
        return 0x8U;
    default:
        return 0x0U;
    }
}

static uint16_t fiat_v2_iv_control(const uint8_t raw[FIAT_V2_WIRE_BYTES], uint8_t combo) {
    const uint16_t cnt = (uint16_t)(fiat_v2_counter(raw) & 0x3FFU);
    if((combo & 0x02U) == 0U) {
        return cnt;
    }
    return (uint16_t)((~cnt) & 0x3FFU);
}

static bool fiat_v2_key_matches_combo(
    const uint8_t raw[FIAT_V2_WIRE_BYTES],
    const uint8_t key[6],
    uint8_t combo) {
    const uint32_t uid = fiat_v2_uid(raw);
    const uint8_t btn = fiat_v2_iv_button(raw, combo);
    const uint16_t ctrl = fiat_v2_iv_control(raw, combo);
    const uint32_t hop = fiat_v2_hop(raw);
    return subghz_protocol_fiat_v1_compute_auth(uid, btn, ctrl, key, 0U) == hop;
}

static void fiat_v2_patch_hop(uint8_t raw[FIAT_V2_WIRE_BYTES], uint32_t hop) {
    if(fiat_v2_is_fca(raw)) {
        raw[10] = (uint8_t)(hop >> 24U);
        raw[11] = (uint8_t)(hop >> 16U);
        raw[12] = (uint8_t)(hop >> 8U);
        raw[13] = (uint8_t)hop;
    } else {
        raw[9] = (uint8_t)(hop >> 24U);
        raw[10] = (uint8_t)(hop >> 16U);
        raw[11] = (uint8_t)(hop >> 8U);
        raw[12] = (uint8_t)hop;
    }
}

static void fiat_v2_patch_button(uint8_t raw[FIAT_V2_WIRE_BYTES], uint8_t selector) {
    raw[7] = (uint8_t)((raw[7] & 0x3FU) | ((selector & 0x03U) << FIAT_V2_BTN_SHIFT));
}

static void fiat_v2_patch_counter(uint8_t raw[FIAT_V2_WIRE_BYTES], uint32_t counter) {
    if(fiat_v2_is_fca(raw)) {
        const uint32_t raw_cnt = (~counter) & 0x3FFFU;
        raw[8] = (uint8_t)((raw_cnt >> 6U) & 0xFFU);
        raw[9] = (uint8_t)((raw[9] & 0x03U) | ((raw_cnt & 0x3FU) << 2U));
    } else {
        const uint32_t raw_cnt = (~counter) & 0x7FFU;
        raw[7] = (uint8_t)((raw[7] & 0xC0U) | ((raw_cnt >> 5U) & 0x3FU));
        raw[8] = (uint8_t)((raw[8] & 0x07U) | ((raw_cnt & 0x1FU) << FIAT_V2_CNT_SHIFT));
    }
}

uint8_t subghz_protocol_fiat_v2_iv_button_for_combo(const uint8_t* raw, uint8_t combo) {
    return fiat_v2_iv_button(raw, combo);
}

uint16_t subghz_protocol_fiat_v2_iv_control_for_combo(const uint8_t* raw, uint8_t combo) {
    return fiat_v2_iv_control(raw, combo);
}

static bool fiat_v2_frame_valid(const uint8_t raw[FIAT_V2_WIRE_BYTES]) {
    if(raw[0] != FIAT_V2_MARKER0 || raw[1] != FIAT_V2_MARKER1) {
        return false;
    }
    if(!fiat_v2_button_valid(raw[7])) {
        return false;
    }

    const uint32_t uid = fiat_v2_uid(raw);
    return uid != 0U && uid != UINT32_MAX;
}

static void fiat_v2_clear_cells(SubGhzProtocolDecoderFiatV2* instance) {
    instance->cell_count = 0U;
    memset(instance->cells, 0, sizeof(instance->cells));
}

static void fiat_v2_decode_fields(SubGhzProtocolDecoderFiatV2* instance) {
    instance->uid = fiat_v2_uid(instance->raw_data);
    instance->button = instance->raw_data[7];
    instance->hop = fiat_v2_hop(instance->raw_data);
    instance->generic.serial = instance->uid;
    instance->generic.btn = instance->button;
    instance->generic.cnt = fiat_v2_counter(instance->raw_data);
    instance->generic.data = ((uint64_t)instance->generic.serial << 32U) | instance->hop;
    instance->generic.data_count_bit = FIAT_V2_LOGICAL_BITS;
    instance->decoder.decode_data = instance->generic.data;
    instance->decoder.decode_count_bit = instance->generic.data_count_bit;
    fiat_v2_verify_hitag2_key(instance);

    if(subghz_custom_btn_get_original() == 0) {
        subghz_custom_btn_set_original(
            (uint8_t)((instance->raw_data[7] >> FIAT_V2_BTN_SHIFT) & 0x03U));
    }
    subghz_custom_btn_set_max(3);
}

static void fiat_v2_verify_hitag2_key(SubGhzProtocolDecoderFiatV2* instance) {
    instance->hitag2_key_valid = false;
    instance->hitag2_epoch = 0U;
    instance->hitag2_iv_combo = 0U;
    memset(instance->hitag2_key, 0, sizeof(instance->hitag2_key));

    const uint8_t(*known_keys)[6] = subghz_protocol_fiat_v1_get_known_keys();

    for(uint8_t i = 0U; i < FIAT_V1_KNOWN_KEY_COUNT; i++) {
        for(uint8_t combo = 0U; combo < FIAT_V2_IV_COMBO_COUNT; combo++) {
            if(fiat_v2_key_matches_combo(instance->raw_data, known_keys[i], combo)) {
                memcpy(instance->hitag2_key, known_keys[i], sizeof(instance->hitag2_key));
                instance->hitag2_key_valid = true;
                instance->hitag2_epoch = 0U;
                instance->hitag2_iv_combo = combo;
                return;
            }
        }
    }
}

static bool fiat_v2_commit(
    SubGhzProtocolDecoderFiatV2* instance,
    const uint8_t raw[FIAT_V2_WIRE_BYTES]) {
    if(!fiat_v2_frame_valid(raw)) {
        return false;
    }

    if(instance->last_raw_valid && memcmp(instance->last_raw_data, raw, FIAT_V2_WIRE_BYTES) == 0) {
        return true;
    }

    memcpy(instance->raw_data, raw, FIAT_V2_WIRE_BYTES);
    memcpy(instance->last_raw_data, raw, FIAT_V2_WIRE_BYTES);
    instance->last_raw_valid = true;
    fiat_v2_decode_fields(instance);

    FURI_LOG_D(
        TAG,
        "Accepted UID:%08lX Btn:%02X Cnt:%02lX Hop:%08lX",
        (unsigned long)instance->uid,
        instance->button,
        (unsigned long)instance->generic.cnt,
        (unsigned long)instance->hop);

    if(instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
    return true;
}

static bool fiat_v2_try_decode_window(SubGhzProtocolDecoderFiatV2* instance, bool invert) {
    if(instance->cell_count != FIAT_V2_WIRE_CELLS) {
        return false;
    }

    uint8_t raw[FIAT_V2_WIRE_BYTES] = {0};
    for(uint8_t bit_index = 0U; bit_index < FIAT_V2_WIRE_BITS; bit_index++) {
        const uint8_t first = instance->cells[bit_index * 2U];
        const uint8_t second = instance->cells[bit_index * 2U + 1U];
        if(first == second) {
            return false;
        }

        bool bit = first != 0U;
        if(invert) {
            bit = !bit;
        }
        if(bit) {
            raw[bit_index >> 3U] |= (uint8_t)(1U << (7U - (bit_index & 7U)));
        }
    }

    return fiat_v2_commit(instance, raw);
}

static void fiat_v2_try_decode(SubGhzProtocolDecoderFiatV2* instance) {
    if(fiat_v2_try_decode_window(instance, false)) {
        return;
    }
    (void)fiat_v2_try_decode_window(instance, true);
}

static void fiat_v2_push_cell(SubGhzProtocolDecoderFiatV2* instance, bool level) {
    if(instance->cell_count < FIAT_V2_WIRE_CELLS) {
        instance->cells[instance->cell_count++] = level ? 1U : 0U;
    } else {
        memmove(instance->cells, &instance->cells[1], FIAT_V2_WIRE_CELLS - 1U);
        instance->cells[FIAT_V2_WIRE_CELLS - 1U] = level ? 1U : 0U;
    }
    fiat_v2_try_decode(instance);
}

static bool fiat_v2_feed_data_pulse(
    SubGhzProtocolDecoderFiatV2* instance,
    bool level,
    uint32_t duration) {
    if(fiat_v2_duration_is_short(duration)) {
        fiat_v2_push_cell(instance, level);
        return true;
    }

    if(fiat_v2_duration_is_long(duration)) {
        fiat_v2_push_cell(instance, level);
        fiat_v2_push_cell(instance, level);
        return true;
    }

    if(!level && duration >= FIAT_V2_BOUNDARY_MIN_US) {
        fiat_v2_push_cell(instance, false);
    }
    fiat_v2_clear_cells(instance);
    return false;
}

void* subghz_protocol_decoder_fiat_v2_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderFiatV2* instance = calloc(1, sizeof(SubGhzProtocolDecoderFiatV2));
    furi_check(instance);
    instance->base.protocol = &fiat_v2_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    subghz_protocol_decoder_fiat_v2_reset(instance);
    return instance;
}

void subghz_protocol_decoder_fiat_v2_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV2* instance = context;

    memset(instance->raw_data, 0, sizeof(instance->raw_data));
    memset(instance->last_raw_data, 0, sizeof(instance->last_raw_data));
    instance->decoder.parser_step = FiatV2DecoderStepReset;
    instance->decoder.decode_data = 0U;
    instance->decoder.decode_count_bit = 0U;
    instance->last_raw_valid = false;
    instance->generic.data = 0U;
    instance->generic.data_count_bit = 0U;
    instance->generic.serial = 0U;
    instance->generic.btn = 0U;
    instance->generic.cnt = 0U;
    instance->uid = 0U;
    instance->hop = 0U;
    instance->button = 0U;
    instance->hitag2_key_valid = false;
    instance->hitag2_epoch = 0U;
    instance->hitag2_iv_combo = 0U;
    memset(instance->hitag2_key, 0, sizeof(instance->hitag2_key));
    fiat_v2_clear_cells(instance);
}

void subghz_protocol_decoder_fiat_v2_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV2* instance = context;

    switch(instance->decoder.parser_step) {
    case FiatV2DecoderStepReset:
        if(fiat_v2_duration_is_short(duration) || fiat_v2_duration_is_long(duration)) {
            fiat_v2_clear_cells(instance);
            instance->decoder.parser_step = FiatV2DecoderStepData;
            (void)fiat_v2_feed_data_pulse(instance, level, duration);
        }
        break;

    case FiatV2DecoderStepData:
        if(!fiat_v2_feed_data_pulse(instance, level, duration)) {
            instance->decoder.parser_step = FiatV2DecoderStepReset;
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_fiat_v2_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV2* instance = context;
    SubGhzBlockDecoder decoder = {
        .decode_data = instance->generic.data,
        .decode_count_bit = 64U,
    };
    return subghz_protocol_blocks_get_hash_data(&decoder, 8U) ^ instance->generic.cnt ^
           instance->button;
}

SubGhzProtocolStatus subghz_protocol_decoder_fiat_v2_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV2* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if(ret != SubGhzProtocolStatusOk) {
        return ret;
    }

    flipper_format_rewind(flipper_format);
    flipper_format_insert_or_update_hex(
        flipper_format, FIAT_V2_RAW_FIELD, instance->raw_data, FIAT_V2_WIRE_BYTES);

    uint32_t hop = instance->hop;
    uint32_t button = instance->button;
    if(!flipper_format_write_uint32(flipper_format, FIAT_V2_HOP_FIELD, &hop, 1) ||
       !flipper_format_write_uint32(flipper_format, FIAT_V2_BTN_FIELD, &button, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    if(!flipper_format_update_uint32(flipper_format, "Serial", &instance->generic.serial, 1)) {
        flipper_format_insert_or_update_uint32(flipper_format, "Serial", &instance->generic.serial, 1);
    }
    uint32_t btn = instance->generic.btn;
    if(!flipper_format_update_uint32(flipper_format, "Btn", &btn, 1)) {
        flipper_format_insert_or_update_uint32(flipper_format, "Btn", &btn, 1);
    }
    uint32_t cnt = instance->generic.cnt;
    if(!flipper_format_update_uint32(flipper_format, "Cnt", &cnt, 1)) {
        flipper_format_insert_or_update_uint32(flipper_format, "Cnt", &cnt, 1);
    }

    if(instance->hitag2_key_valid) {
        uint32_t epoch = instance->hitag2_epoch & 0x3FFFFUL;
        uint32_t iv_combo = instance->hitag2_iv_combo;
        if(!flipper_format_insert_or_update_hex(
               flipper_format, FIAT_V2_HITAG2_KEY_FIELD, instance->hitag2_key, 6U) ||
           !flipper_format_write_uint32(flipper_format, FIAT_V2_HITAG2_EPOCH_FIELD, &epoch, 1) ||
           !flipper_format_write_uint32(flipper_format, FIAT_V2_HITAG2_IV_FIELD, &iv_combo, 1)) {
            return SubGhzProtocolStatusErrorParserOthers;
        }
    }
    return SubGhzProtocolStatusOk;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_fiat_v2_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV2* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_fiat_v2_const.min_count_bit_for_found);
    if(ret != SubGhzProtocolStatusOk) {
        return ret;
    }
    if(instance->generic.data_count_bit != FIAT_V2_LOGICAL_BITS) {
        return SubGhzProtocolStatusErrorValueBitCount;
    }

    flipper_format_rewind(flipper_format);
    if(flipper_format_read_hex(
           flipper_format, FIAT_V2_RAW_FIELD, instance->raw_data, FIAT_V2_WIRE_BYTES)) {
        if(!fiat_v2_frame_valid(instance->raw_data)) {
            return SubGhzProtocolStatusErrorParserOthers;
        }
        fiat_v2_decode_fields(instance);

        uint8_t key[6] = {0};
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_hex(flipper_format, FIAT_V2_HITAG2_KEY_FIELD, key, 6U)) {
            uint32_t iv_combo = 0U;
            flipper_format_rewind(flipper_format);
            if(!flipper_format_read_uint32(
                   flipper_format, FIAT_V2_HITAG2_IV_FIELD, &iv_combo, 1U)) {
                iv_combo = 0U;
            }
            iv_combo &= 0x03U;
            if(fiat_v2_key_matches_combo(instance->raw_data, key, (uint8_t)iv_combo)) {
                memcpy(instance->hitag2_key, key, sizeof(instance->hitag2_key));
                instance->hitag2_key_valid = true;
                instance->hitag2_epoch = 0U;
                instance->hitag2_iv_combo = (uint8_t)iv_combo;
            }
        }
        return SubGhzProtocolStatusOk;
    }

    return SubGhzProtocolStatusErrorParserOthers;
}

void subghz_protocol_decoder_fiat_v2_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV2* instance = context;

    if(instance->hitag2_key_valid) {
        subghz_custom_btn_set_max(3);
        uint8_t original_selector =
            (uint8_t)((instance->button >> FIAT_V2_BTN_SHIFT) & 0x03U);
        uint8_t display_selector =
            fiat_v2_ui_selector(subghz_custom_btn_get(), original_selector);
        uint8_t display_btn =
            (uint8_t)((instance->button & 0x3FU) | ((display_selector & 0x03U) << FIAT_V2_BTN_SHIFT));
        furi_string_cat_printf(
            output,
            "%s %ubit\r\n"
            "Key:%02X%02X%02X%02X%02X%02X\r\n"
            "SN:0x%lX Btn:[%s]\r\n"
            "Cnt:%02lX\r\n",
            instance->generic.protocol_name,
            FIAT_V2_LOGICAL_BITS,
            instance->hitag2_key[0],
            instance->hitag2_key[1],
            instance->hitag2_key[2],
            instance->hitag2_key[3],
            instance->hitag2_key[4],
            instance->hitag2_key[5],
            (unsigned long)instance->uid,
            fiat_v2_button_name(display_btn),
            (unsigned long)instance->generic.cnt);
    } else {
        furi_string_cat_printf(
            output,
            "%s %ubit\r\n"
            "Key:?\r\n"
            "SN:0x%lX Btn:[%s]\r\n"
            "Cnt:%02lX\r\n",
            instance->generic.protocol_name,
            FIAT_V2_LOGICAL_BITS,
            (unsigned long)instance->uid,
            fiat_v2_button_name(instance->button),
            (unsigned long)instance->generic.cnt);
    }
}
