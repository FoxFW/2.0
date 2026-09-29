#include "keeloq.h"
#include "keeloq_common.h"

#include "../subghz_keystore.h"
#include <m-array.h>

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/encoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#include "../blocks/custom_btn_i.h"
#include "../subghz_keystore_i.h"

#define TAG "SubGhzProtocolKeeloq"

static bool bypass = false;

static const SubGhzBlockConst subghz_protocol_keeloq_const = {
    .te_short = 400,
    .te_long = 800,
    .te_delta = 140,
    .min_count_bit_for_found = 64,
};

struct SubGhzProtocolDecoderKeeloq {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t header_count;
    SubGhzKeystore* keystore;
    const char* manufacture_name;

    FuriString* manufacture_from_file;
};

struct SubGhzProtocolEncoderKeeloq {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    SubGhzKeystore* keystore;
    const char* manufacture_name;

    FuriString* manufacture_from_file;
};

typedef enum {
    KeeloqDecoderStepReset = 0,
    KeeloqDecoderStepCheckPreambula,
    KeeloqDecoderStepSaveDuration,
    KeeloqDecoderStepCheckDuration,
} KeeloqDecoderStep;

static uint8_t keeloq_counter_mode = 0;

static const char* const keeloq_shifted_btn_brands[] = {
    "Nissan",
    "Suzuki",
    "Pandora_SUZUKI",
    "DoorHan",
    "Pandora_DEA",
    "Pandora_GIBIDI",
    "Pandora_MCODE",
    "Pandora_Unknown_1",
    "Pandora_Unknown_2",
    "Alligator_S-275",
    "Pantera_XS/Jaguar",
    "APS-1100_APS-2550",
};

static uint8_t keeloq_btn_get_position(const char* mfname, uint8_t btn_code) {
    for(size_t i = 0; i < COUNT_OF(keeloq_shifted_btn_brands); i++) {
        if(strcmp(mfname, keeloq_shifted_btn_brands[i]) == 0) {
            switch(btn_code) {
            case 0x2: return 1;
            case 0x4: return 2;
            case 0x8: return 3;
            case 0x1: return 4;
            default:  return 0;
            }
        }
    }
    return 0;
}

const SubGhzProtocolDecoder subghz_protocol_keeloq_decoder = {
    .alloc = subghz_protocol_decoder_keeloq_alloc,
    .free = subghz_protocol_decoder_keeloq_free,

    .feed = subghz_protocol_decoder_keeloq_feed,
    .reset = subghz_protocol_decoder_keeloq_reset,

    .get_hash_data = subghz_protocol_decoder_keeloq_get_hash_data,
    .serialize = subghz_protocol_decoder_keeloq_serialize,
    .deserialize = subghz_protocol_decoder_keeloq_deserialize,
    .get_string = subghz_protocol_decoder_keeloq_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_keeloq_encoder = {
    .alloc = subghz_protocol_encoder_keeloq_alloc,
    .free = subghz_protocol_encoder_keeloq_free,

    .deserialize = subghz_protocol_encoder_keeloq_deserialize,
    .stop = subghz_protocol_encoder_keeloq_stop,
    .yield = subghz_protocol_encoder_keeloq_yield,
};

const SubGhzProtocol subghz_protocol_keeloq = {
    .name = SUBGHZ_PROTOCOL_KEELOQ_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_868 | SubGhzProtocolFlag_315 |
            SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,

    .decoder = &subghz_protocol_keeloq_decoder,
    .encoder = &subghz_protocol_keeloq_encoder,
};

static uint32_t subghz_protocol_keeloq_check_remote_controller(
    SubGhzBlockGeneric* instance,
    SubGhzKeystore* keystore,
    const char** manufacture_name);

static uint8_t subghz_protocol_keeloq_get_btn_code(uint8_t last_btn_code);

void* subghz_protocol_encoder_keeloq_alloc(SubGhzEnvironment* environment) {
    SubGhzProtocolEncoderKeeloq* instance = malloc(sizeof(SubGhzProtocolEncoderKeeloq));

    instance->base.protocol = &subghz_protocol_keeloq;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->keystore = subghz_environment_get_keystore(environment);

    instance->encoder.repeat = 3;
    instance->encoder.size_upload = 1100;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_running = false;

    instance->manufacture_from_file = furi_string_alloc();

    return instance;
}

void subghz_protocol_encoder_keeloq_free(void* context) {
    furi_assert(context);
    SubGhzProtocolEncoderKeeloq* instance = context;
    furi_string_free(instance->manufacture_from_file);
    free(instance->encoder.upload);
    free(instance);
}

static bool subghz_protocol_keeloq_gen_data(
    SubGhzProtocolEncoderKeeloq* instance,
    uint8_t btn,
    bool counter_up,
    bool skip_btn_check) {

    if(instance->manufacture_name == 0x0) {
        instance->manufacture_name = "";
    }

    ProgMode prog_mode = subghz_custom_btn_get_prog_mode();
    if(!skip_btn_check && (keeloq_counter_mode != 7)) {

        if(subghz_custom_btn_get_original() == 0) {
            subghz_custom_btn_set_original(btn);
        }

        if(prog_mode == PROG_MODE_KEELOQ_BFT) {
            instance->manufacture_name = "BFT";
        } else if(prog_mode == PROG_MODE_KEELOQ_APRIMATIC) {
            instance->manufacture_name = "Aprimatic";
        } else if(prog_mode == PROG_MODE_KEELOQ_DEA_MIO) {
            instance->manufacture_name = "Dea_Mio";
        }

        uint8_t klq_last_custom_btn = 0xA;
        if((strcmp(instance->manufacture_name, "BFT") == 0) ||
           (strcmp(instance->manufacture_name, "Aprimatic") == 0) ||
           (strcmp(instance->manufacture_name, "Dea_Mio") == 0) ||
           (strcmp(instance->manufacture_name, "NICE_MHOUSE") == 0)) {
            klq_last_custom_btn = 0xF;
        } else if(
            (strcmp(instance->manufacture_name, "FAAC_RC,XT") == 0) ||
            (strcmp(instance->manufacture_name, "Monarch") == 0) ||
            (strcmp(instance->manufacture_name, "NICE_Smilo") == 0)) {
            klq_last_custom_btn = 0xB;
        } else if(
            (strcmp(instance->manufacture_name, "Novoferm") == 0) ||
            (strcmp(instance->manufacture_name, "Stilmatic") == 0)) {
            klq_last_custom_btn = 0x9;
        } else if(
            (strcmp(instance->manufacture_name, "EcoStar") == 0) ||
            (strcmp(instance->manufacture_name, "Sommer") == 0)) {
            klq_last_custom_btn = 0x6;
        } else if((strcmp(instance->manufacture_name, "AN-Motors") == 0)) {
            klq_last_custom_btn = 0xC;
        } else if((strcmp(instance->manufacture_name, "Cardin_S449") == 0)) {
            klq_last_custom_btn = 0xD;
        }

        btn = subghz_protocol_keeloq_get_btn_code(klq_last_custom_btn);
    }

    if(subghz_block_generic_global_button_override_get(&btn))
        FURI_LOG_D(TAG, "Button sucessfully changed to 0x%X", btn);

    uint32_t fix = (uint32_t)btn << 28 | instance->generic.serial;
    uint32_t hop = 0;
    uint64_t man = 0;
    uint64_t code_found_reverse;
    int res = 0;

    if(strcmp(instance->manufacture_name, "BFT") == 0) {

        if(btn == 0xF) {
            prog_mode = PROG_MODE_KEELOQ_BFT;
        } else if(prog_mode == PROG_MODE_KEELOQ_BFT) {
            prog_mode = PROG_MODE_OFF;
        }
    } else if(strcmp(instance->manufacture_name, "Aprimatic") == 0) {

        if(btn == 0xF) {
            prog_mode = PROG_MODE_KEELOQ_APRIMATIC;
        } else if(prog_mode == PROG_MODE_KEELOQ_APRIMATIC) {
            prog_mode = PROG_MODE_OFF;
        }
    } else if(strcmp(instance->manufacture_name, "Dea_Mio") == 0) {

        if(btn == 0xF) {
            prog_mode = PROG_MODE_KEELOQ_DEA_MIO;
        } else if(prog_mode == PROG_MODE_KEELOQ_DEA_MIO) {
            prog_mode = PROG_MODE_OFF;
        }
    }
    subghz_custom_btn_set_prog_mode(prog_mode);

    if(prog_mode == PROG_MODE_KEELOQ_BFT) {
        hop = instance->generic.seed;
    } else if(prog_mode == PROG_MODE_KEELOQ_APRIMATIC) {

        hop = 0x1A2B3C4D;
    } else if(prog_mode == PROG_MODE_KEELOQ_DEA_MIO) {

        hop = 0x00000000;
    }
    if(counter_up && prog_mode == PROG_MODE_OFF) {

        if(keeloq_counter_mode == 0 || bypass) {

            if(furi_hal_subghz_get_rolling_counter_mult() != -0x7FFFFFFF || bypass) {
                bypass = false;

                if(!subghz_block_generic_global_counter_override_get(&instance->generic.cnt)) {

                    if((instance->generic.cnt + furi_hal_subghz_get_rolling_counter_mult()) >
                       0xFFFF) {
                        instance->generic.cnt = 0;
                    } else {
                        instance->generic.cnt += furi_hal_subghz_get_rolling_counter_mult();
                    }
                }
            } else {
                if((instance->generic.cnt + 0x1) > 0xFFFF) {
                    instance->generic.cnt = 0;
                } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
                    instance->generic.cnt = 0xFFFE;
                } else {
                    instance->generic.cnt++;
                }
            }
        } else if(keeloq_counter_mode == 1) {

            if((instance->generic.cnt + 0x1) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else if(instance->generic.cnt >= 0x1 && instance->generic.cnt != 0xFFFE) {
                instance->generic.cnt = 0xFFFE;
            } else {
                instance->generic.cnt++;
            }
        } else if(keeloq_counter_mode == 2) {

            if((instance->generic.cnt + 0x3333) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt += 0x3333;
            }
        } else if(keeloq_counter_mode == 3) {

            if(instance->generic.cnt != 0x8006 && instance->generic.cnt != 0x8007 &&
               instance->generic.cnt != 0x0006) {
                instance->generic.cnt = 0x8006;
            } else if(instance->generic.cnt == 0x8007) {
                instance->generic.cnt = 0x0006;
            } else {
                instance->generic.cnt++;
            }

        } else if(keeloq_counter_mode == 4) {

            if(instance->generic.cnt != 0x807B && instance->generic.cnt != 0x807C &&
               instance->generic.cnt != 0x007B) {
                instance->generic.cnt = 0x807B;
            } else if(instance->generic.cnt == 0x807C) {
                instance->generic.cnt = 0x007B;
            } else {
                instance->generic.cnt++;
            }
        } else if(keeloq_counter_mode == 5) {

            if((instance->generic.cnt + 0x1) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt = 0xFFFF;
            }
        } else if(keeloq_counter_mode == 6) {

        } else {

            if((instance->generic.cnt + 0x3333) > 0xFFFF) {
                instance->generic.cnt = 0;
            } else {
                instance->generic.cnt += 0x3333;
            }
        }
    }
    if(prog_mode == PROG_MODE_OFF) {

        if(strcmp(instance->manufacture_name, "Unknown") == 0) {

            code_found_reverse = subghz_protocol_blocks_reverse_key(
                instance->generic.data, instance->generic.data_count_bit);
            hop = code_found_reverse & 0x00000000ffffffff;
        } else if(strcmp(instance->manufacture_name, "AN-Motors") == 0) {

            hop = (instance->generic.cnt & 0xFF) << 24 | (instance->generic.cnt & 0xFF) << 16 |
                  (btn & 0xF) << 12 | 0x404;
        } else if(strcmp(instance->manufacture_name, "HCS101") == 0) {

            hop = instance->generic.cnt << 16 | (btn & 0xF) << 12 | 0x000;
        } else {

            uint32_t decrypt = (uint32_t)btn << 28 |
                               (instance->generic.serial & 0x3FF)
                                   << 16 |
                               instance->generic.cnt;

            if(strcmp(instance->manufacture_name, "Aprimatic") == 0) {

                uint32_t apri_serial = instance->generic.serial;
                uint8_t apr1 = 0;
                for(uint16_t i = 1; i != 0b10000000000; i <<= 1) {
                    if(apri_serial & i) apr1++;
                }
                apri_serial &= 0b00001111111111;
                if(apr1 % 2 == 0) {
                    apri_serial |= 0b110000000000;
                }
                decrypt = btn << 28 | (apri_serial & 0xFFF) << 16 | instance->generic.cnt;
            } else if(
                (strcmp(instance->manufacture_name, "DTM_Neo") == 0) ||
                (strcmp(instance->manufacture_name, "FAAC_RC,XT") == 0) ||
                (strcmp(instance->manufacture_name, "Mutanco_Mutancode") == 0) ||
                (strcmp(instance->manufacture_name, "Came_Space") == 0) ||
                (strcmp(instance->manufacture_name, "Genius_Bravo") == 0) ||
                (strcmp(instance->manufacture_name, "GSN") == 0) ||
                (strcmp(instance->manufacture_name, "Rosh") == 0) ||
                (strcmp(instance->manufacture_name, "Rossi") == 0) ||
                (strcmp(instance->manufacture_name, "Pecinin") == 0) ||
                (strcmp(instance->manufacture_name, "Steelmate") == 0) ||
                (strcmp(instance->manufacture_name, "Cardin_S449") == 0) ||
                (strcmp(instance->manufacture_name, "Stilmatic") == 0)) {

                decrypt = btn << 28 | (instance->generic.serial & 0xFFF) << 16 |
                          instance->generic.cnt;
            } else if(
                (strcmp(instance->manufacture_name, "NICE_Smilo") == 0) ||
                (strcmp(instance->manufacture_name, "NICE_MHOUSE") == 0) ||
                (strcmp(instance->manufacture_name, "JCM_Tech") == 0)) {

                decrypt = btn << 28 | (instance->generic.serial & 0xFF) << 16 |
                          instance->generic.cnt;
            } else if(
                (strcmp(instance->manufacture_name, "Beninca") == 0) ||
                (strcmp(instance->manufacture_name, "Merlin") == 0)) {
                decrypt = btn << 28 | (0x000) << 16 | instance->generic.cnt;

            } else if(strcmp(instance->manufacture_name, "Centurion") == 0) {
                decrypt = btn << 28 | (0x1CE) << 16 | instance->generic.cnt;

            } else if(strcmp(instance->manufacture_name, "Monarch") == 0) {
                decrypt = btn << 28 | (0x100) << 16 | instance->generic.cnt;

            } else if(strcmp(instance->manufacture_name, "Dea_Mio") == 0) {
                uint8_t first_disc_num = (instance->generic.serial >> 8) & 0xF;
                uint8_t result_disc = (0xC + (first_disc_num % 4));
                uint32_t dea_serial = (instance->generic.serial & 0xFF) |
                                      (((uint32_t)result_disc) << 8);
                decrypt = btn << 28 | (dea_serial & 0xFFF) << 16 | instance->generic.cnt;

            }

            uint8_t kl_type_en = instance->keystore->kl_type;
            for
                M_EACH(
                    manufacture_code,
                    *subghz_keystore_get_data(instance->keystore),
                    SubGhzKeyArray_t) {
                    res = strcmp(
                        furi_string_get_cstr(manufacture_code->name), instance->manufacture_name);
                    if(res == 0) {
                        switch(manufacture_code->type) {
                        case KEELOQ_LEARNING_SIMPLE:

                            hop = subghz_protocol_keeloq_common_encrypt(
                                decrypt, manufacture_code->key);
                            break;
                        case KEELOQ_LEARNING_NORMAL:

                            man = subghz_protocol_keeloq_common_normal_learning(
                                fix, manufacture_code->key);
                            hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            break;
                        case KEELOQ_LEARNING_SECURE:

                            man = subghz_protocol_keeloq_common_secure_learning(
                                fix, instance->generic.seed, manufacture_code->key);
                            hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            break;
                        case KEELOQ_LEARNING_MAGIC_XOR_TYPE_1:

                            man = subghz_protocol_keeloq_common_magic_xor_type1_learning(
                                instance->generic.serial, manufacture_code->key);
                            hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            break;
                        case KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_1:

                            man = subghz_protocol_keeloq_common_magic_serial_type1_learning(
                                fix, manufacture_code->key);
                            hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            break;
                        case KEELOQ_LEARNING_UNKNOWN:
                            if(kl_type_en == 1) {
                                hop = subghz_protocol_keeloq_common_encrypt(
                                    decrypt, manufacture_code->key);
                            }
                            if(kl_type_en == 2) {
                                man = subghz_protocol_keeloq_common_normal_learning(
                                    fix, manufacture_code->key);
                                hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            }
                            if(kl_type_en == 3) {
                                man = subghz_protocol_keeloq_common_secure_learning(
                                    fix, instance->generic.seed, manufacture_code->key);
                                hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            }
                            if(kl_type_en == 4) {
                                man = subghz_protocol_keeloq_common_magic_xor_type1_learning(
                                    instance->generic.serial, manufacture_code->key);
                                hop = subghz_protocol_keeloq_common_encrypt(decrypt, man);
                            }
                            break;
                        }
                        break;
                    }
                }
        }
    }
    if(hop || (prog_mode == PROG_MODE_KEELOQ_DEA_MIO) || (prog_mode == PROG_MODE_KEELOQ_BFT)) {

        uint64_t yek = (uint64_t)fix << 32 | hop;
        instance->generic.data =
            subghz_protocol_blocks_reverse_key(yek, instance->generic.data_count_bit);
    }
    return true;
}

bool subghz_protocol_keeloq_create_data(
    void* context,
    FlipperFormat* flipper_format,
    uint32_t serial,
    uint8_t btn,
    uint16_t cnt,
    const char* manufacture_name,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolEncoderKeeloq* instance = context;
    instance->generic.serial = serial;
    instance->generic.cnt = cnt;
    instance->manufacture_name = manufacture_name;
    instance->generic.data_count_bit = 64;
    if(subghz_protocol_keeloq_gen_data(instance, btn, false, true)) {
        return (
            subghz_block_generic_serialize(&instance->generic, flipper_format, preset) ==
            SubGhzProtocolStatusOk);
    }
    return false;
}

bool subghz_protocol_keeloq_bft_create_data(
    void* context,
    FlipperFormat* flipper_format,
    uint32_t serial,
    uint8_t btn,
    uint16_t cnt,
    uint32_t seed,
    const char* manufacture_name,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolEncoderKeeloq* instance = context;
    instance->generic.serial = serial;
    instance->generic.btn = btn;
    instance->generic.cnt = cnt;
    instance->generic.seed = seed;
    instance->manufacture_name = manufacture_name;
    instance->generic.data_count_bit = 64;

    if(subghz_protocol_keeloq_gen_data(instance, btn, false, true)) {
        return (
            subghz_block_generic_serialize(&instance->generic, flipper_format, preset) ==
            SubGhzProtocolStatusOk);
    }
    return false;
}

static size_t subghz_protocol_encoder_keeloq_encode_to_timings(
    SubGhzProtocolEncoderKeeloq* instance,
    uint8_t btn,
    bool counter_up,
    size_t index) {
    furi_assert(instance);

    if(!subghz_protocol_keeloq_gen_data(instance, btn, counter_up, false)) {
        return 0;
    }

    uint32_t gap_duration = subghz_protocol_keeloq_const.te_short * 40;
    if((strcmp(instance->manufacture_name, "Sommer") == 0)) {
        gap_duration = subghz_protocol_keeloq_const.te_short * 29;
    }

    for(uint8_t i = 11; i > 0; i--) {
        instance->encoder.upload[index++] =
            level_duration_make(true, (uint32_t)subghz_protocol_keeloq_const.te_short);
        instance->encoder.upload[index++] =
            level_duration_make(false, (uint32_t)subghz_protocol_keeloq_const.te_short);
    }
    instance->encoder.upload[index++] =
        level_duration_make(true, (uint32_t)subghz_protocol_keeloq_const.te_short);
    instance->encoder.upload[index++] =
        level_duration_make(false, (uint32_t)subghz_protocol_keeloq_const.te_short * 10);

    for(uint8_t i = instance->generic.data_count_bit; i > 0; i--) {
        if(bit_read(instance->generic.data, i - 1)) {

            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)subghz_protocol_keeloq_const.te_short);
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)subghz_protocol_keeloq_const.te_long);
        } else {

            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)subghz_protocol_keeloq_const.te_long);
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)subghz_protocol_keeloq_const.te_short);
        }
    }

    instance->encoder.upload[index++] =
        level_duration_make(true, (uint32_t)subghz_protocol_keeloq_const.te_short);
    instance->encoder.upload[index++] =
        level_duration_make(false, (uint32_t)subghz_protocol_keeloq_const.te_long);

    instance->encoder.upload[index++] =
        level_duration_make(true, (uint32_t)subghz_protocol_keeloq_const.te_short);
    instance->encoder.upload[index++] = level_duration_make(false, gap_duration);

    return index;
}

static bool
    subghz_protocol_encoder_keeloq_get_upload(SubGhzProtocolEncoderKeeloq* instance, uint8_t btn) {
    furi_assert(instance);

    instance->encoder.size_upload = 0;
    size_t upindex = 0;

    if(subghz_block_generic_global.cnt_need_override ||
       subghz_block_generic_global.btn_need_override)
        bypass = true;

    if(keeloq_counter_mode == 7 && !bypass) {
        uint16_t temp_cnt = instance->generic.cnt;
        instance->encoder.repeat = 1;
        for(uint8_t i = 7; i > 0; i--) {
            if(i == 3) {
                instance->generic.cnt = 0x0000;
                upindex = subghz_protocol_encoder_keeloq_encode_to_timings(
                    instance, (uint8_t)0x00, false, upindex);
                continue;
            } else if(i == 2) {
                instance->generic.cnt = temp_cnt;
                upindex = subghz_protocol_encoder_keeloq_encode_to_timings(
                    instance, btn, false, upindex);
                continue;
            } else if(i == 1) {
                instance->generic.cnt = temp_cnt + 1;
                upindex = subghz_protocol_encoder_keeloq_encode_to_timings(
                    instance, btn, false, upindex);
                continue;
            }
            upindex = subghz_protocol_encoder_keeloq_encode_to_timings(
                instance, (uint8_t)0x00, true, upindex);
        }
        instance->encoder.size_upload = upindex;
        return true;
    } else {
        instance->encoder.repeat = 3;
        instance->encoder.size_upload =
            subghz_protocol_encoder_keeloq_encode_to_timings(instance, btn, true, upindex);
    }

    return true;
}

SubGhzProtocolStatus
    subghz_protocol_encoder_keeloq_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolEncoderKeeloq* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        ret = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            subghz_protocol_keeloq_const.min_count_bit_for_found);
        if(ret != SubGhzProtocolStatusOk) {
            break;
        }
        if(instance->generic.data_count_bit !=
           subghz_protocol_keeloq_const.min_count_bit_for_found) {
            FURI_LOG_E(TAG, "Wrong number of bits in key");
            break;
        }

        uint8_t seed_data[sizeof(uint32_t)] = {0};
        for(size_t i = 0; i < sizeof(uint32_t); i++) {
            seed_data[sizeof(uint32_t) - i - 1] = (instance->generic.seed >> i * 8) & 0xFF;
        }
        if(!flipper_format_read_hex(flipper_format, "Seed", seed_data, sizeof(uint32_t))) {
            FURI_LOG_D(TAG, "ENCODER: Missing Seed");
        }
        instance->generic.seed = seed_data[0] << 24 | seed_data[1] << 16 | seed_data[2] << 8 |
                                 seed_data[3];

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        if(flipper_format_read_string(
               flipper_format, "Manufacture", instance->manufacture_from_file)) {
            instance->manufacture_name = furi_string_get_cstr(instance->manufacture_from_file);
            instance->keystore->mfname = instance->manufacture_name;

            if(strcmp(instance->manufacture_name, "Sommer(fsk476)") == 0) {
                instance->manufacture_name = "Sommer";
                instance->keystore->mfname = instance->manufacture_name;
                if(!flipper_format_rewind(flipper_format)) {
                    FURI_LOG_E(TAG, "Rewind error");
                    break;
                }
                if(!flipper_format_update_string_cstr(
                       flipper_format, "Manufacture", instance->manufacture_name)) {
                    FURI_LOG_E(TAG, "DECODER: Unable to fix Sommer manufacture name");
                    ret = SubGhzProtocolStatusError;
                    break;
                }
            }
        } else {
            FURI_LOG_D(TAG, "ENCODER: Missing Manufacture");
        }

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        uint32_t tmp_counter_mode;
        if(flipper_format_read_uint32(flipper_format, "CounterMode", &tmp_counter_mode, 1)) {
            keeloq_counter_mode = (uint8_t)tmp_counter_mode;
        } else {
            keeloq_counter_mode = 0;
        }

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        subghz_protocol_keeloq_check_remote_controller(
            &instance->generic, instance->keystore, &instance->manufacture_name);

        flipper_format_read_uint32(
            flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1);

        if(!subghz_protocol_encoder_keeloq_get_upload(instance, instance->generic.btn)) {
            ret = SubGhzProtocolStatusErrorEncoderGetUpload;
            break;
        }
        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            ret = SubGhzProtocolStatusErrorParserOthers;
            break;
        }
        uint8_t key_data[sizeof(uint64_t)] = {0};
        for(size_t i = 0; i < sizeof(uint64_t); i++) {
            key_data[sizeof(uint64_t) - i - 1] = (instance->generic.data >> (i * 8)) & 0xFF;
        }
        if(!flipper_format_update_hex(flipper_format, "Key", key_data, sizeof(uint64_t))) {
            FURI_LOG_E(TAG, "Unable to add Key");
            ret = SubGhzProtocolStatusErrorParserKey;
            break;
        }
        instance->encoder.front = 0;
        instance->encoder.is_running = true;
    } while(false);

    return ret;
}

void subghz_protocol_encoder_keeloq_stop(void* context) {
    SubGhzProtocolEncoderKeeloq* instance = context;
    instance->encoder.is_running = false;
    instance->encoder.front = 0;
}

LevelDuration subghz_protocol_encoder_keeloq_yield(void* context) {
    SubGhzProtocolEncoderKeeloq* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
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

void* subghz_protocol_decoder_keeloq_alloc(SubGhzEnvironment* environment) {
    SubGhzProtocolDecoderKeeloq* instance = malloc(sizeof(SubGhzProtocolDecoderKeeloq));
    instance->base.protocol = &subghz_protocol_keeloq;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->keystore = subghz_environment_get_keystore(environment);
    instance->manufacture_from_file = furi_string_alloc();

    subghz_custom_btn_set_prog_mode(PROG_MODE_OFF);

    return instance;
}

void subghz_protocol_decoder_keeloq_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;
    furi_string_free(instance->manufacture_from_file);

    free(instance);
}

void subghz_protocol_decoder_keeloq_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;
    instance->decoder.parser_step = KeeloqDecoderStepReset;

    instance->keystore->mfname = "";
    instance->keystore->kl_type = 0;
}

void subghz_protocol_decoder_keeloq_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;

    switch(instance->decoder.parser_step) {
    case KeeloqDecoderStepReset:
        if((level) && DURATION_DIFF(duration, subghz_protocol_keeloq_const.te_short) <
                          subghz_protocol_keeloq_const.te_delta) {
            instance->decoder.parser_step = KeeloqDecoderStepCheckPreambula;
            instance->header_count++;
        }
        break;
    case KeeloqDecoderStepCheckPreambula:
        if((!level) && (DURATION_DIFF(duration, subghz_protocol_keeloq_const.te_short) <
                        subghz_protocol_keeloq_const.te_delta)) {
            instance->decoder.parser_step = KeeloqDecoderStepReset;
            break;
        }
        if((instance->header_count > 2) &&
           (DURATION_DIFF(duration, subghz_protocol_keeloq_const.te_short * 10) <
            subghz_protocol_keeloq_const.te_delta * 10)) {

            instance->decoder.parser_step = KeeloqDecoderStepSaveDuration;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        } else {
            instance->decoder.parser_step = KeeloqDecoderStepReset;
            instance->header_count = 0;
        }
        break;
    case KeeloqDecoderStepSaveDuration:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = KeeloqDecoderStepCheckDuration;
        }
        break;
    case KeeloqDecoderStepCheckDuration:
        if(!level) {
            if(duration >= ((uint32_t)subghz_protocol_keeloq_const.te_short * 2 +
                            subghz_protocol_keeloq_const.te_delta)) {

                instance->decoder.parser_step = KeeloqDecoderStepReset;
                if((instance->decoder.decode_count_bit >=
                    subghz_protocol_keeloq_const.min_count_bit_for_found) &&
                   (instance->decoder.decode_count_bit <=
                    subghz_protocol_keeloq_const.min_count_bit_for_found + 2)) {
                    if(instance->generic.data != instance->decoder.decode_data) {
                        instance->generic.data = instance->decoder.decode_data;
                        instance->generic.data_count_bit =
                            subghz_protocol_keeloq_const.min_count_bit_for_found;
                        if(instance->base.callback)
                            instance->base.callback(&instance->base, instance->base.context);
                    }
                    instance->decoder.decode_data = 0;
                    instance->decoder.decode_count_bit = 0;
                    instance->header_count = 0;
                }
                break;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_keeloq_const.te_short) <
                 subghz_protocol_keeloq_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_keeloq_const.te_long) <
                 subghz_protocol_keeloq_const.te_delta * 2)) {
                if(instance->decoder.decode_count_bit <
                   subghz_protocol_keeloq_const.min_count_bit_for_found) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                } else {
                    instance->decoder.decode_count_bit++;
                }
                instance->decoder.parser_step = KeeloqDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_keeloq_const.te_long) <
                 subghz_protocol_keeloq_const.te_delta * 2) &&
                (DURATION_DIFF(duration, subghz_protocol_keeloq_const.te_short) <
                 subghz_protocol_keeloq_const.te_delta)) {
                if(instance->decoder.decode_count_bit <
                   subghz_protocol_keeloq_const.min_count_bit_for_found) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                } else {
                    instance->decoder.decode_count_bit++;
                }
                instance->decoder.parser_step = KeeloqDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = KeeloqDecoderStepReset;
                instance->header_count = 0;
            }
        } else {
            instance->decoder.parser_step = KeeloqDecoderStepReset;
            instance->header_count = 0;
        }
        break;
    }
}

static inline bool subghz_protocol_keeloq_check_decrypt(
    SubGhzBlockGeneric* instance,
    uint32_t decrypt,
    uint8_t btn,
    uint32_t end_serial) {
    furi_assert(instance);
    if((decrypt >> 28 == btn) && (((((uint16_t)(decrypt >> 16)) & 0xFF) == end_serial) ||
                                  ((((uint16_t)(decrypt >> 16)) & 0xFF) == 0))) {
        instance->cnt = decrypt & 0x0000FFFF;

        return true;
    }
    return false;
}

static inline bool subghz_protocol_keeloq_check_decrypt_centurion(
    SubGhzBlockGeneric* instance,
    uint32_t decrypt,
    uint8_t btn) {
    furi_assert(instance);

    if((decrypt >> 28 == btn) && ((((uint16_t)(decrypt >> 16)) & 0x3FF) == 0x1CE)) {
        instance->cnt = decrypt & 0x0000FFFF;

        return true;
    }
    return false;
}

static uint32_t subghz_protocol_keeloq_check_remote_controller_selector(
    SubGhzBlockGeneric* instance,
    uint32_t fix,
    uint32_t hop,
    SubGhzKeystore* keystore,
    const char** manufacture_name) {

    uint16_t end_serial = (uint16_t)(fix & 0xFF);
    uint8_t btn = (uint8_t)(fix >> 28);
    uint32_t decrypt = 0;
    uint64_t man;
    bool mf_not_set = false;

    const char* mfname = keystore->mfname;

    if(strcmp(mfname, "Unknown") == 0) {
        return 0;
    } else if(strcmp(mfname, "") == 0) {
        mf_not_set = true;
    }
    for
        M_EACH(manufacture_code, *subghz_keystore_get_data(keystore), SubGhzKeyArray_t) {
            if(mf_not_set || (strcmp(furi_string_get_cstr(manufacture_code->name), mfname) == 0)) {
                switch(manufacture_code->type) {
                case KEELOQ_LEARNING_SIMPLE:

                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, manufacture_code->key);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        return decrypt;
                    }
                    break;
                case KEELOQ_LEARNING_NORMAL:

                    man =
                        subghz_protocol_keeloq_common_normal_learning(fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if((strcmp(furi_string_get_cstr(manufacture_code->name), "Centurion") == 0)) {
                        if(subghz_protocol_keeloq_check_decrypt_centurion(instance, decrypt, btn)) {
                            *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                            keystore->mfname = *manufacture_name;
                            return decrypt;
                        }
                    } else {
                        if(subghz_protocol_keeloq_check_decrypt(
                               instance, decrypt, btn, end_serial)) {
                            *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                            keystore->mfname = *manufacture_name;
                            return decrypt;
                        }
                    }
                    break;
                case KEELOQ_LEARNING_SECURE:
                    bool reset_seed_back = false;
                    if((strcmp(furi_string_get_cstr(manufacture_code->name), "BFT") == 0)) {

                        instance->seed = (fix & 0xFFFFFFF);
                        reset_seed_back = true;

                    }
                    man = subghz_protocol_keeloq_common_secure_learning(
                        fix, instance->seed, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        return decrypt;
                    } else {
                        if(reset_seed_back) instance->seed = 0;

                        man = subghz_protocol_keeloq_common_secure_learning(
                            fix, instance->seed, manufacture_code->key);
                        decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                        if(subghz_protocol_keeloq_check_decrypt(
                               instance, decrypt, btn, end_serial)) {
                            *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                            keystore->mfname = *manufacture_name;
                            return decrypt;
                        }
                    }
                    break;
                case KEELOQ_LEARNING_MAGIC_XOR_TYPE_1:
                    man = subghz_protocol_keeloq_common_magic_xor_type1_learning(
                        fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        return decrypt;
                    }
                    break;
                case KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_1:
                    man = subghz_protocol_keeloq_common_magic_serial_type1_learning(
                        fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        return decrypt;
                    }
                    break;
                case KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_2:
                    man = subghz_protocol_keeloq_common_magic_serial_type2_learning(
                        fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        return decrypt;
                    }
                    break;
                case KEELOQ_LEARNING_MAGIC_SERIAL_TYPE_3:
                    man = subghz_protocol_keeloq_common_magic_serial_type3_learning(
                        fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        return decrypt;
                    }
                    break;
                case KEELOQ_LEARNING_UNKNOWN:

                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, manufacture_code->key);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 1;
                        return decrypt;
                    }

                    uint64_t man_rev = 0;
                    uint64_t man_rev_byte = 0;
                    for(uint8_t i = 0; i < 64; i += 8) {
                        man_rev_byte = (uint8_t)(manufacture_code->key >> i);
                        man_rev = man_rev | man_rev_byte << (56 - i);
                    }

                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man_rev);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 1;
                        return decrypt;
                    }

                    man =
                        subghz_protocol_keeloq_common_normal_learning(fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 2;
                        return decrypt;
                    }

                    man = subghz_protocol_keeloq_common_normal_learning(fix, man_rev);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 2;
                        return decrypt;
                    }

                    man = subghz_protocol_keeloq_common_secure_learning(
                        fix, instance->seed, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 3;
                        return decrypt;
                    }

                    man = subghz_protocol_keeloq_common_secure_learning(
                        fix, instance->seed, man_rev);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 3;
                        return decrypt;
                    }

                    man = subghz_protocol_keeloq_common_magic_xor_type1_learning(
                        fix, manufacture_code->key);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 4;
                        return decrypt;
                    }

                    man = subghz_protocol_keeloq_common_magic_xor_type1_learning(fix, man_rev);
                    decrypt = subghz_protocol_keeloq_common_decrypt(hop, man);
                    if(subghz_protocol_keeloq_check_decrypt(instance, decrypt, btn, end_serial)) {
                        *manufacture_name = furi_string_get_cstr(manufacture_code->name);
                        keystore->mfname = *manufacture_name;
                        keystore->kl_type = 4;
                        return decrypt;
                    }

                    break;
                }
            }
        }

    *manufacture_name = "Unknown";
    keystore->mfname = "Unknown";
    instance->cnt = 0;

    return 0;
}

static uint32_t subghz_protocol_keeloq_check_remote_controller(
    SubGhzBlockGeneric* instance,
    SubGhzKeystore* keystore,
    const char** manufacture_name) {

    uint64_t key = subghz_protocol_blocks_reverse_key(instance->data, instance->data_count_bit);
    uint32_t key_fix = key >> 32;
    uint32_t key_hop = key & 0x00000000ffffffff;
    static uint16_t temp_counter = 0;
    uint32_t resdecrypt = 0;

    ProgMode prog_mode = subghz_custom_btn_get_prog_mode();
    if(prog_mode == PROG_MODE_OFF) {
        if(keystore->mfname == 0x0) {
            keystore->mfname = "";
        }
        if(*manufacture_name == 0x0) {
            *manufacture_name = "";
        }

        if(strlen(keystore->mfname) < 1) {

            if((key_hop >> 24) == ((key_hop >> 16) & 0x00ff) &&
               (key_fix >> 28) == ((key_hop >> 12) & 0x0f) && (key_hop & 0xFFF) == 0x404) {
                *manufacture_name = "AN-Motors";
                keystore->mfname = *manufacture_name;
                instance->cnt = key_hop >> 16;
            } else if((key_hop & 0xFFF) == (0x000) && (key_fix >> 28) == ((key_hop >> 12) & 0x0f)) {
                *manufacture_name = "HCS101";
                keystore->mfname = *manufacture_name;
                instance->cnt = key_hop >> 16;
            } else {
                resdecrypt = subghz_protocol_keeloq_check_remote_controller_selector(
                    instance, key_fix, key_hop, keystore, manufacture_name);
            }
        } else {

            if(strcmp(keystore->mfname, "AN-Motors") == 0) {

                *manufacture_name = "AN-Motors";
                keystore->mfname = *manufacture_name;
                instance->cnt = key_hop >> 16;
            } else if(strcmp(keystore->mfname, "HCS101") == 0) {

                *manufacture_name = "HCS101";
                keystore->mfname = *manufacture_name;
                instance->cnt = key_hop >> 16;
            } else {

                resdecrypt = subghz_protocol_keeloq_check_remote_controller_selector(
                    instance, key_fix, key_hop, keystore, manufacture_name);
            }
        }

        temp_counter = instance->cnt;

    } else if(prog_mode == PROG_MODE_KEELOQ_BFT) {

        *manufacture_name = "BFT";
        keystore->mfname = *manufacture_name;
        instance->cnt = temp_counter;
    } else if(prog_mode == PROG_MODE_KEELOQ_APRIMATIC) {

        *manufacture_name = "Aprimatic";
        keystore->mfname = *manufacture_name;
        instance->cnt = temp_counter;
    } else if(prog_mode == PROG_MODE_KEELOQ_DEA_MIO) {

        *manufacture_name = "Dea_Mio";
        keystore->mfname = *manufacture_name;
        instance->cnt = temp_counter;
    } else {

        furi_crash("Unsupported Prog Mode");
    }

    instance->serial = key_fix & 0x0FFFFFFF;
    instance->btn = key_fix >> 28;

    if(subghz_custom_btn_get_original() == 0) {
        subghz_custom_btn_set_original(instance->btn);
    }

    subghz_custom_btn_set_max(4);

    return resdecrypt;
}

uint8_t subghz_protocol_decoder_keeloq_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_keeloq_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;

    SubGhzProtocolStatus res =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    subghz_protocol_keeloq_check_remote_controller(
        &instance->generic, instance->keystore, &instance->manufacture_name);

    if(strcmp(instance->manufacture_name, "BFT") == 0) {
        uint8_t seed_data[sizeof(uint32_t)] = {0};
        for(size_t i = 0; i < sizeof(uint32_t); i++) {
            seed_data[sizeof(uint32_t) - i - 1] = (instance->generic.seed >> i * 8) & 0xFF;
        }
        if((res == SubGhzProtocolStatusOk) &&
           !flipper_format_write_hex(flipper_format, "Seed", seed_data, sizeof(uint32_t))) {
            FURI_LOG_E(TAG, "DECODER Serialize: Unable to add Seed");
            res = SubGhzProtocolStatusError;
        }
        instance->generic.seed = seed_data[0] << 24 | seed_data[1] << 16 | seed_data[2] << 8 |
                                 seed_data[3];
    }

    if((res == SubGhzProtocolStatusOk) &&
       !flipper_format_write_string_cstr(
           flipper_format, "Manufacture", instance->manufacture_name)) {
        FURI_LOG_E(TAG, "DECODER Serialize: Unable to add manufacture name");
        res = SubGhzProtocolStatusError;
    }
    return res;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_keeloq_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;
    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    do {
        if(SubGhzProtocolStatusOk !=
           subghz_block_generic_deserialize(&instance->generic, flipper_format)) {
            FURI_LOG_E(TAG, "Deserialize error");
            break;
        }
        if(instance->generic.data_count_bit !=
           subghz_protocol_keeloq_const.min_count_bit_for_found) {
            FURI_LOG_E(TAG, "Wrong number of bits in key");
            break;
        }

        uint8_t seed_data[sizeof(uint32_t)] = {0};
        for(size_t i = 0; i < sizeof(uint32_t); i++) {
            seed_data[sizeof(uint32_t) - i - 1] = (instance->generic.seed >> i * 8) & 0xFF;
        }
        if(!flipper_format_read_hex(flipper_format, "Seed", seed_data, sizeof(uint32_t))) {
            FURI_LOG_D(TAG, "DECODER: Missing Seed");
        }
        instance->generic.seed = seed_data[0] << 24 | seed_data[1] << 16 | seed_data[2] << 8 |
                                 seed_data[3];

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        if(flipper_format_read_string(
               flipper_format, "Manufacture", instance->manufacture_from_file)) {
            instance->manufacture_name = furi_string_get_cstr(instance->manufacture_from_file);
            instance->keystore->mfname = instance->manufacture_name;

            if(strcmp(instance->manufacture_name, "Sommer(fsk476)") == 0) {
                instance->manufacture_name = "Sommer";
                instance->keystore->mfname = instance->manufacture_name;
                if(!flipper_format_rewind(flipper_format)) {
                    FURI_LOG_E(TAG, "Rewind error");
                    break;
                }
                if(!flipper_format_update_string_cstr(
                       flipper_format, "Manufacture", instance->manufacture_name)) {
                    FURI_LOG_E(TAG, "DECODER: Unable to fix Sommer manufacture name");
                    res = SubGhzProtocolStatusError;
                    break;
                }
            }
        } else {
            FURI_LOG_D(TAG, "DECODER: Missing Manufacture");
        }

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        uint32_t tmp_counter_mode;
        if(flipper_format_read_uint32(flipper_format, "CounterMode", &tmp_counter_mode, 1)) {
            keeloq_counter_mode = (uint8_t)tmp_counter_mode;
        } else {
            keeloq_counter_mode = 0;
        }

        if(!flipper_format_rewind(flipper_format)) {
            FURI_LOG_E(TAG, "Rewind error");
            break;
        }

        res = SubGhzProtocolStatusOk;
    } while(false);

    return res;
}

static uint8_t subghz_protocol_keeloq_get_btn_code(uint8_t last_btn_code) {
    uint8_t custom_btn_id = subghz_custom_btn_get();
    uint8_t original_btn_code = subghz_custom_btn_get_original();
    uint8_t btn = original_btn_code;

    if(last_btn_code == 0) {
        last_btn_code = 0xA;
    }

    if((custom_btn_id == SUBGHZ_CUSTOM_BTN_OK) && (original_btn_code != 0)) {

        btn = original_btn_code;
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_UP) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0x2;
            break;
        case 0x2:
            btn = 0x1;
            break;
        case 0xA:
            btn = 0x1;
            break;
        case 0x4:
            btn = 0x1;
            break;
        case 0x8:
            btn = 0x1;
            break;
        case 0xF:
            btn = 0x1;
            break;
        case 0x9:
            btn = 0x2;
            break;
        case 0x6:
            btn = 0x2;
            break;

        default:
            btn = 0x1;
            break;
        }
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_DOWN) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0x4;
            break;
        case 0x2:
            btn = 0x4;
            break;
        case 0xA:
            btn = 0x4;
            break;
        case 0x4:
            btn = last_btn_code;
            break;
        case 0x8:
            btn = 0x4;
            break;
        case 0xF:
            btn = 0x4;
            break;
        case 0x9:
            btn = 0x4;
            break;
        case 0x6:
            btn = 0x4;
            break;

        default:
            btn = 0x4;
            break;
        }
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_LEFT) {
        switch(original_btn_code) {
        case 0x1:
            btn = 0x8;
            break;
        case 0x2:
            btn = 0x8;
            break;
        case 0xA:
            btn = 0x8;
            break;
        case 0x4:
            btn = 0x8;
            break;
        case 0x8:
            btn = 0x2;
            break;
        case 0xF:
            btn = 0x8;
            break;
        case 0x9:
            btn = 0x6;
            break;
        case 0x6:
            btn = 0x9;
            break;

        default:
            btn = 0x8;
            break;
        }
    } else if(custom_btn_id == SUBGHZ_CUSTOM_BTN_RIGHT) {
        switch(original_btn_code) {
        case 0x1:
            btn = last_btn_code;
            break;
        case 0x2:
            btn = last_btn_code;
            break;
        case 0xA:
            btn = 0x2;
            break;
        case 0x4:
            btn = 0x2;
            break;
        case 0x8:
            btn = last_btn_code;
            break;
        case 0xF:
            btn = 0x2;
            break;
        case 0x9:
            btn = last_btn_code;
            break;
        case 0x6:
            btn = last_btn_code;
            break;

        default:
            btn = last_btn_code;
            break;
        }
    }

    return btn;
}

void subghz_protocol_decoder_keeloq_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderKeeloq* instance = context;

    uint32_t hopdecrypt = 0;

    hopdecrypt = subghz_protocol_keeloq_check_remote_controller(
        &instance->generic, instance->keystore, &instance->manufacture_name);

    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    uint64_t code_found_reverse = subghz_protocol_blocks_reverse_key(
        instance->generic.data, instance->generic.data_count_bit);
    uint32_t code_found_reverse_hi = code_found_reverse >> 32;
    uint32_t code_found_reverse_lo = code_found_reverse & 0x00000000ffffffff;

    if(strcmp(instance->manufacture_name, "BFT") == 0) {

        subghz_block_generic_global.cnt_is_available = true;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global.current_cnt = instance->generic.cnt;

        subghz_block_generic_global.btn_is_available = true;
        subghz_block_generic_global.current_btn = instance->generic.btn;
        subghz_block_generic_global.btn_length_bit = 4;

        ProgMode prog_mode = subghz_custom_btn_get_prog_mode();
        if(prog_mode == PROG_MODE_KEELOQ_BFT) {
            furi_string_cat_printf(
                output,
                "%s %dbit\r\n"
                "Key:%08lX%08lX\r\n"
                "Fix:0x%08lX    Cnt:%04lX\r\n"
                "Hop:0x%08lX    Btn:%01X\r\n"
                "MF:%s PRG Sd:%08lX",
                instance->generic.protocol_name,
                instance->generic.data_count_bit,
                code_found_hi,
                code_found_lo,
                code_found_reverse_hi,
                instance->generic.cnt,
                code_found_reverse_lo,
                instance->generic.btn,
                instance->manufacture_name,
                instance->generic.seed);
        } else {
            furi_string_cat_printf(
                output,
                "%s %dbit\r\n"
                "Key:%08lX%08lX\r\n"
                "Fix:0x%08lX    Cnt:%04lX\r\n"
                "Hop:0x%08lX    Btn:%01X\r\n"
                "MF:%s Sd:%08lX",
                instance->generic.protocol_name,
                instance->generic.data_count_bit,
                code_found_hi,
                code_found_lo,
                code_found_reverse_hi,
                instance->generic.cnt,
                hopdecrypt,
                instance->generic.btn,
                instance->manufacture_name,
                instance->generic.seed);
        }
    } else if(strcmp(instance->manufacture_name, "Unknown") == 0) {
        subghz_block_generic_global.btn_is_available = true;
        subghz_block_generic_global.current_btn = instance->generic.btn;
        subghz_block_generic_global.btn_length_bit = 4;
        instance->generic.cnt = 0x0;
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:%08lX%08lX\r\n"
            "Fix:0x%08lX    Cnt:????\r\n"
            "Hop:0x%08lX    Btn:%01X\r\n"
            "MF:%s",
            instance->generic.protocol_name,
            instance->generic.data_count_bit,
            code_found_hi,
            code_found_lo,
            code_found_reverse_hi,
            code_found_reverse_lo,
            instance->generic.btn,
            instance->manufacture_name);
    } else {
        subghz_block_generic_global.cnt_is_available = true;
        subghz_block_generic_global.cnt_length_bit = 16;
        subghz_block_generic_global.current_cnt = instance->generic.cnt;
        subghz_block_generic_global.btn_is_available = true;
        subghz_block_generic_global.current_btn = instance->generic.btn;
        subghz_block_generic_global.btn_length_bit = 4;
        uint8_t btn_pos =
            keeloq_btn_get_position(instance->manufacture_name, instance->generic.btn);
        if(btn_pos > 0) {
            furi_string_cat_printf(
                output,
                "%s %dbit\r\n"
                "Key:%08lX%08lX\r\n"
                "Fix:0x%08lX    Cnt:%04lX\r\n"
                "Hop:0x%08lX  Btn:%lX(B%lu)\r\n"
                "MF:%s",
                instance->generic.protocol_name,
                instance->generic.data_count_bit,
                code_found_hi,
                code_found_lo,
                code_found_reverse_hi,
                instance->generic.cnt,
                hopdecrypt,
                (uint32_t)instance->generic.btn,
                (uint32_t)btn_pos,
                instance->manufacture_name);
        } else {
            furi_string_cat_printf(
                output,
                "%s %dbit\r\n"
                "Key:%08lX%08lX\r\n"
                "Fix:0x%08lX    Cnt:%04lX\r\n"
                "Hop:0x%08lX    Btn:%01X\r\n"
                "MF:%s",
                instance->generic.protocol_name,
                instance->generic.data_count_bit,
                code_found_hi,
                code_found_lo,
                code_found_reverse_hi,
                instance->generic.cnt,
                hopdecrypt,
                instance->generic.btn,
                instance->manufacture_name);
        }
    }
}
