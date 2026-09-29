#pragma once

#include "subratt_protocols.h"
#include "helpers/subratt_radio_device_loader.h"

#include <lib/subghz/protocols/base.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/environment.h>

#define SUBRATT_MAX_LEN_NAME 64
#define SUBRATT_PATH         EXT_PATH("subghz")
#define SUBRATT_FILE_EXT     ".sub"

#define SUBRATT_KEYLOG_EXT ".sbk"
#define SUBRATT_KEYLOG_SESSION_PATH EXT_PATH("subghz/.subratt_session.sbk")

typedef enum {
    SubRattFileResultUnknown,
    SubRattFileResultOk,
    SubRattFileResultErrorOpenFile,
    SubRattFileResultMissingOrIncorrectHeader,
    SubRattFileResultFrequencyNotAllowed,
    SubRattFileResultMissingOrIncorrectFrequency,
    SubRattFileResultPresetInvalid,
    SubRattFileResultMissingProtocol,
    SubRattFileResultProtocolNotSupported,
    SubRattFileResultDynamicProtocolNotValid,
    SubRattFileResultProtocolNotFound,
    SubRattFileResultMissingOrIncorrectBit,
    SubRattFileResultMissingOrIncorrectKey,
    SubRattFileResultMissingOrIncorrectTe,
} SubRattFileResult;

typedef struct {
    const SubRattProtocol* protocol_info;
    SubRattProtocol* file_protocol_info;

    uint64_t current_step;

    SubGhzReceiver* receiver;
    SubGhzProtocolDecoderBase* decoder_result;
    SubGhzEnvironment* environment;
    const SubGhzDevice* radio_device;

    SubRattAttacks attack;
    uint64_t max_value;
    uint8_t extra_repeats;

    uint64_t key_from_file;
    uint64_t current_key_from_file;
    bool two_bytes;
    uint8_t opencode;
    uint8_t bit_index;

    uint32_t replay_frequency;
    FuriHalSubGhzPreset replay_preset;
    SubRattFileProtocol replay_file;
    uint8_t replay_bits;
    uint32_t replay_te;
    uint8_t replay_repeat;
    uint8_t replay_opencode;
    bool replay_is_file_attack;
    uint8_t replay_load_index;
    uint64_t replay_file_key;
    bool replay_two_bytes;
    uint32_t replay_total_keys;
    FuriString* replay_file_path;
} SubRattDevice;

static char* const error_device_ok = "OK";
static char* const error_device_invalid_path = "invalid name/path";
static char* const error_device_missing_header = "Missing or incorrect header";
static char* const error_device_invalid_frequency = "Invalid frequency!";
static char* const error_device_incorrect_frequency = "Missing or incorrect Frequency";
static char* const error_device_preset_fail = "Preset FAIL";
static char* const error_device_missing_protocol = "Missing Protocol";
static char* const error_device_protocol_unsupported = "Protocol unsupported";
static char* const error_device_dynamic_protocol_unsupported = "Dynamic protocol unsupported";
static char* const error_device_protocol_not_found = "Protocol not found";
static char* const error_device_missing_bit = "Missing or incorrect Bit";
static char* const error_device_missing_key = "Missing or incorrect Key";
static char* const error_device_missing_te = "Missing or incorrect TE";
static char* const error_device_unknown = "Unknown error";

SubRattDevice* subratt_device_alloc(const SubGhzDevice* radio_device);

void subratt_device_free(SubRattDevice* instance);

bool subratt_device_save_file(SubRattDevice* instance, const char* key_name);

const char* subratt_device_error_get_desc(SubRattFileResult error_id);
SubRattFileResult subratt_device_attack_set(
    SubRattDevice* context,
    SubRattAttacks type,
    uint8_t extra_repeats);

uint8_t subratt_device_load_from_file(SubRattDevice* context, const char* file_path);

SubRattFileResult
    subratt_device_load_keylog_from_file(SubRattDevice* instance, const char* file_path);

uint64_t subratt_device_add_step(SubRattDevice* instance, int8_t step);

void subratt_device_free_protocol_info(SubRattDevice* instance);

void subratt_device_attack_set_default_values(
    SubRattDevice* context,
    SubRattAttacks default_attack);
