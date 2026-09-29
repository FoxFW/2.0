#pragma once

#include <furi.h>
#include <furi_hal_subghz.h>
#include <core/string.h>
#include <toolbox/stream/stream.h>

#define SUBRATT_PROTOCOL_MAX_REPEATS 9

#define SUBRATT_DUAL_MODE_MAX_BITS 25

typedef enum {
    CAMEFileProtocol,
    NICEFileProtocol,
    ChamberlainFileProtocol,
    LinearFileProtocol,
    LinearDeltaFileProtocol,
    PrincetonFileProtocol,
    RAWFileProtocol,
    BETTFileProtocol,
    ClemsaFileProtocol,
    DoitrandFileProtocol,
    GateTXFileProtocol,
    MagellanFileProtocol,
    IntertechnoV3FileProtocol,
    AnsonicFileProtocol,
    SMC5326FileProtocol,
    UNILARMFileProtocol,
    PT2260FileProtocol,
    PT2262FileProtocol,
    HoneywellFileProtocol,
    HoltekFileProtocol,
    LegrandFileProtocol,

    HollarmFileProtocol,
    GangQiFileProtocol,
    Marantec24FileProtocol,
    FeronFileProtocol,
    MegaCodeFileProtocol,

    MastercodeFileProtocol,
    X10FileProtocol,
    UnknownFileProtocol,
    TotalFileProtocol,
} SubRattFileProtocol;

typedef enum {
    SubRattAttackCAME12bit303,
    SubRattAttackCAME12bit307,
    SubRattAttackCAME12bit315,
    SubRattAttackCAME12bit330,
    SubRattAttackCAME12bit433,
    SubRattAttackCAME12bit868,
    SubRattAttackNICE12bit433,
    SubRattAttackNICE12bit868,
    SubRattAttackAnsonic12bit433075,
    SubRattAttackAnsonic12bit433,
    SubRattAttackAnsonic12bit434,
    SubRattAttackHoltek12bitFM433,
    SubRattAttackHoltek12bitAM433,
    SubRattAttackHoltek12bitAM315,
    SubRattAttackHoltek12bitAM868,
    SubRattAttackHoltek12bitAM915,
    SubRattAttackChamberlain9bit300,
    SubRattAttackChamberlain9bit315,
    SubRattAttackChamberlain9bit318,
    SubRattAttackChamberlain9bit390,
    SubRattAttackChamberlain9bit433,
    SubRattAttackChamberlain8bit300,
    SubRattAttackChamberlain8bit315,
    SubRattAttackChamberlain8bit390,
    SubRattAttackChamberlain7bit300,
    SubRattAttackChamberlain7bit315,
    SubRattAttackChamberlain7bit390,
    SubRattAttackLinear10bit300,
    SubRattAttackLinear10bit310,
    SubRattAttackLinearDelta8bit310,
    SubRattAttackUNILARM24bit330,
    SubRattAttackUNILARM24bit433,
    SubRattAttackSMC532624bit330,
    SubRattAttackSMC532624bit433,
    SubRattAttackPT226024bit315,
    SubRattAttackPT226024bit330,
    SubRattAttackPT226024bit390,
    SubRattAttackPT226024bit433,
    SubRattAttackPT226224bit315,
    SubRattAttackPT226224bit418,
    SubRattAttackPT226224bit430,
    SubRattAttackPT226224bit4305,
    SubRattAttackPT226224bit433,
    SubRattAttackGateTX24bit433,
    SubRattAttackMarantec2424bit868,
    SubRattAttackLegrand18bit433,
    SubRattAttackClemsa18bit433,
    SubRattAttackBett18bit433,
    SubRattAttackMegaCode24bit315,

    SubRattAttackHoneywell64bit345,
    SubRattAttackHollarm42bit433,
    SubRattAttackGangQi34bit433,
    SubRattAttackMagellan32bit433,
    SubRattAttackIntertechnoV332bit433,
    SubRattAttackFeron32bit433,
    SubRattAttackDoitrand37bit433,
    SubRattAttackMastercode36bit433,
    SubRattAttackX10_32bit310,
    SubRattAttackLoadFile,

    SubRattAttackLoadSavedKeys,
    SubRattAttackTotalCount,
} SubRattAttacks;

typedef enum {
    SubRattBrandCAME,
    SubRattBrandNICE,
    SubRattBrandAnsonic,
    SubRattBrandHoltek,
    SubRattBrandChamberlain,
    SubRattBrandLinear,
    SubRattBrandUNILARM,
    SubRattBrandSMC5326,
    SubRattBrandPT2260,
    SubRattBrandPT2262,
    SubRattBrandGateTX,
    SubRattBrandMarantec24,
    SubRattBrandLegrand,
    SubRattBrandClemsa,
    SubRattBrandBett,
    SubRattBrandMegaCode,

    SubRattBrandHoneywell,
    SubRattBrandHollarm,
    SubRattBrandGangQi,
    SubRattBrandMagellan,
    SubRattBrandIntertechnoV3,
    SubRattBrandFeron,
    SubRattBrandDoitrand,
    SubRattBrandMastercode,
    SubRattBrandX10,
    SubRattBrandLoadFile,

    SubRattBrandLoadSavedKeys,
    SubRattBrandCount,
} SubRattBrand;

typedef struct {
    const char* name;
    const SubRattAttacks* attacks;
    uint8_t attack_count;
} SubRattTypeGroup;

typedef struct {
    const char* name;
    const SubRattTypeGroup* types;
    uint8_t type_count;
} SubRattBrandGroup;

typedef struct {
    uint32_t frequency;
    uint8_t bits;
    uint32_t te;
    uint8_t repeat;
    uint8_t opencode;
    FuriHalSubGhzPreset preset;
    SubRattFileProtocol file;
} SubRattProtocol;

const SubRattProtocol* subratt_protocol(SubRattAttacks index);

const char* subratt_protocol_preset(FuriHalSubGhzPreset preset);

const char* subratt_protocol_file(SubRattFileProtocol protocol);

FuriHalSubGhzPreset subratt_protocol_convert_preset(FuriString* preset_name);

SubRattFileProtocol subratt_protocol_file_protocol_name(FuriString* name);

uint8_t subratt_protocol_repeats_count(SubRattAttacks index);

const char* subratt_protocol_name(SubRattAttacks index);

void subratt_protocol_default_payload(
    Stream* stream,
    SubRattFileProtocol file,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t repeat,
    uint8_t opencode);

void subratt_protocol_file_payload(
    Stream* stream,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t repeat,
    uint8_t bit_index,
    uint64_t file_key,
    bool two_bytes);

void subratt_protocol_default_generate_file(
    Stream* stream,
    uint32_t frequency,
    FuriHalSubGhzPreset preset,
    SubRattFileProtocol file,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t opencode);

void subratt_protocol_file_generate_file(
    Stream* stream,
    uint32_t frequency,
    FuriHalSubGhzPreset preset,
    SubRattFileProtocol file,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t bit_index,
    uint64_t file_key,
    bool two_bytes);

uint64_t
    subratt_protocol_calc_max_value(SubRattAttacks attack_type, uint8_t bits, bool two_bytes);

const SubRattBrandGroup* subratt_brand_group(SubRattBrand brand);

uint64_t subratt_protocol_random_step(SubRattFileProtocol file, uint8_t bits);

const char* subratt_protocol_freq_name(SubRattAttacks index);

void subratt_protocol_find_brand_type(
    SubRattAttacks attack,
    uint8_t* brand,
    uint8_t* type,
    uint8_t* freq_idx);

const char* subratt_protocol_title(SubRattAttacks attack);
