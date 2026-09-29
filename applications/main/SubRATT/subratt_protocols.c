#include "subratt_protocols.h"
#include <stdio.h>
#include "math.h"
#include <furi_hal_random.h>

#define TAG "SubRattProtocols"

const SubRattProtocol subratt_protocol_came_12bit_303 = {
    .frequency = 303875000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = CAMEFileProtocol};

const SubRattProtocol subratt_protocol_came_12bit_307 = {
    .frequency = 307800000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = CAMEFileProtocol};

const SubRattProtocol subratt_protocol_came_12bit_315 = {
    .frequency = 315000000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = CAMEFileProtocol};

const SubRattProtocol subratt_protocol_came_12bit_330 = {
    .frequency = 330000000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = CAMEFileProtocol};

const SubRattProtocol subratt_protocol_came_12bit_433 = {
    .frequency = 433920000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = CAMEFileProtocol};

const SubRattProtocol subratt_protocol_came_12bit_868 = {
    .frequency = 868350000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = CAMEFileProtocol};

const SubRattProtocol subratt_protocol_nice_12bit_433 = {
    .frequency = 433920000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = NICEFileProtocol};

const SubRattProtocol subratt_protocol_nice_12bit_868 = {
    .frequency = 868350000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = NICEFileProtocol};

const SubRattProtocol subratt_protocol_ansonic_12bit_433075 = {
    .frequency = 433075000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPreset2FSKDev238Async,
    .file = AnsonicFileProtocol};

const SubRattProtocol subratt_protocol_ansonic_12bit_433 = {
    .frequency = 433920000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPreset2FSKDev238Async,
    .file = AnsonicFileProtocol};

const SubRattProtocol subratt_protocol_ansonic_12bit_434 = {
    .frequency = 434075000,
    .bits = 12,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPreset2FSKDev238Async,
    .file = AnsonicFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_9bit_300 = {
    .frequency = 300000000,
    .bits = 9,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_9bit_315 = {
    .frequency = 315000000,
    .bits = 9,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_9bit_318 = {
    .frequency = 318000000,
    .bits = 9,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_9bit_390 = {
    .frequency = 390000000,
    .bits = 9,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_9bit_433 = {
    .frequency = 433920000,
    .bits = 9,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_8bit_300 = {
    .frequency = 300000000,
    .bits = 8,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_8bit_315 = {
    .frequency = 315000000,
    .bits = 8,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_8bit_390 = {
    .frequency = 390000000,
    .bits = 8,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_7bit_300 = {
    .frequency = 300000000,
    .bits = 7,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_7bit_315 = {
    .frequency = 315000000,
    .bits = 7,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_chamberlain_7bit_390 = {
    .frequency = 390000000,
    .bits = 7,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ChamberlainFileProtocol};

const SubRattProtocol subratt_protocol_linear_10bit_300 = {
    .frequency = 300000000,
    .bits = 10,
    .te = 0,
    .repeat = 5,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = LinearFileProtocol};

const SubRattProtocol subratt_protocol_linear_10bit_310 = {
    .frequency = 310000000,
    .bits = 10,
    .te = 0,
    .repeat = 5,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = LinearFileProtocol};

const SubRattProtocol subratt_protocol_linear_delta_8bit_310 = {
    .frequency = 310000000,
    .bits = 8,
    .te = 0,
    .repeat = 5,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = LinearDeltaFileProtocol};

const SubRattProtocol subratt_protocol_unilarm_24bit_330 = {
    .frequency = 330000000,
    .bits = 25,
    .te = 209,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = UNILARMFileProtocol};

const SubRattProtocol subratt_protocol_unilarm_24bit_433 = {
    .frequency = 433920000,
    .bits = 25,
    .te = 209,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = UNILARMFileProtocol};

const SubRattProtocol subratt_protocol_smc5326_24bit_330 = {
    .frequency = 330000000,
    .bits = 25,
    .te = 320,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = SMC5326FileProtocol};

const SubRattProtocol subratt_protocol_smc5326_24bit_433 = {
    .frequency = 433920000,
    .bits = 25,
    .te = 320,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = SMC5326FileProtocol};

const SubRattProtocol subratt_protocol_pt2260_24bit_315 = {
    .frequency = 315000000,
    .bits = 24,
    .te = 286,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2260FileProtocol};

const SubRattProtocol subratt_protocol_pt2260_24bit_330 = {
    .frequency = 330000000,
    .bits = 24,
    .te = 286,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2260FileProtocol};

const SubRattProtocol subratt_protocol_pt2260_24bit_390 = {
    .frequency = 390000000,
    .bits = 24,
    .te = 286,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2260FileProtocol};

const SubRattProtocol subratt_protocol_pt2260_24bit_433 = {
    .frequency = 433920000,
    .bits = 24,
    .te = 286,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2260FileProtocol};

const SubRattProtocol subratt_protocol_pt2262_24bit_315 = {
    .frequency = 315000000,
    .bits = 24,
    .te = 350,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2262FileProtocol};

const SubRattProtocol subratt_protocol_pt2262_24bit_418 = {
    .frequency = 418000000,
    .bits = 24,
    .te = 350,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2262FileProtocol};

const SubRattProtocol subratt_protocol_pt2262_24bit_430 = {
    .frequency = 430000000,
    .bits = 24,
    .te = 350,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2262FileProtocol};

const SubRattProtocol subratt_protocol_pt2262_24bit_430_5 = {
    .frequency = 430500000,
    .bits = 24,
    .te = 350,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2262FileProtocol};

const SubRattProtocol subratt_protocol_pt2262_24bit_433 = {
    .frequency = 433920000,
    .bits = 24,
    .te = 350,
    .repeat = 4,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = PT2262FileProtocol};

const SubRattProtocol subratt_protocol_holtek_12bit_433 = {
    .frequency = 433920000,
    .bits = 12,
    .te = 204,
    .repeat = 4,
    .preset = FuriHalSubGhzPreset2FSKDev476Async,
    .file = HoltekFileProtocol};

const SubRattProtocol subratt_protocol_holtek_12bit_am_433 = {
    .frequency = 433920000,
    .bits = 12,
    .te = 433,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = HoltekFileProtocol};

const SubRattProtocol subratt_protocol_holtek_12bit_am_315 = {
    .frequency = 315000000,
    .bits = 12,
    .te = 433,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = HoltekFileProtocol};

const SubRattProtocol subratt_protocol_holtek_12bit_am_868 = {
    .frequency = 868350000,
    .bits = 12,
    .te = 433,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = HoltekFileProtocol};

const SubRattProtocol subratt_protocol_holtek_12bit_am_915 = {
    .frequency = 915000000,
    .bits = 12,
    .te = 433,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = HoltekFileProtocol};

const SubRattProtocol subratt_protocol_gate_tx_24bit_433 = {
    .frequency = 433920000,
    .bits = 24,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = GateTXFileProtocol};

const SubRattProtocol subratt_protocol_marantec24_24bit_868 = {
    .frequency = 868350000,
    .bits = 24,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = Marantec24FileProtocol};

const SubRattProtocol subratt_protocol_legrand_18bit_433 = {
    .frequency = 433920000,
    .bits = 18,
    .te = 375,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = LegrandFileProtocol};

const SubRattProtocol subratt_protocol_clemsa_18bit_433 = {
    .frequency = 433920000,
    .bits = 18,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = ClemsaFileProtocol};

const SubRattProtocol subratt_protocol_bett_18bit_433 = {
    .frequency = 433920000,
    .bits = 18,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = BETTFileProtocol};

const SubRattProtocol subratt_protocol_megacode_24bit_315 = {
    .frequency = 315000000,
    .bits = 24,
    .te = 0,
    .repeat = 3,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = MegaCodeFileProtocol};

const SubRattProtocol subratt_protocol_honeywell_64bit_345 = {
    .frequency = 345000000,
    .bits = 64,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = HoneywellFileProtocol};

const SubRattProtocol subratt_protocol_hollarm_42bit_433 = {
    .frequency = 433920000,
    .bits = 42,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = HollarmFileProtocol};

const SubRattProtocol subratt_protocol_gangqi_34bit_433 = {
    .frequency = 433920000,
    .bits = 34,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = GangQiFileProtocol};

const SubRattProtocol subratt_protocol_magellan_32bit_433 = {
    .frequency = 433920000,
    .bits = 32,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = MagellanFileProtocol};

const SubRattProtocol subratt_protocol_intertechno_v3_32bit_433 = {
    .frequency = 433920000,
    .bits = 32,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = IntertechnoV3FileProtocol};

const SubRattProtocol subratt_protocol_feron_32bit_433 = {
    .frequency = 433920000,
    .bits = 32,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = FeronFileProtocol};

const SubRattProtocol subratt_protocol_doitrand_37bit_433 = {
    .frequency = 433920000,
    .bits = 37,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = DoitrandFileProtocol};

const SubRattProtocol subratt_protocol_mastercode_36bit_433 = {
    .frequency = 433920000,
    .bits = 36,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = MastercodeFileProtocol};

const SubRattProtocol subratt_protocol_x10_32bit_310 = {
    .frequency = 310000000,
    .bits = 32,
    .te = 0,
    .repeat = 3,
    .opencode = 0,
    .preset = FuriHalSubGhzPresetOok650Async,
    .file = X10FileProtocol};

const SubRattProtocol subratt_protocol_load_file =
    {0, 0, 0, 3, 0, FuriHalSubGhzPresetOok650Async, UnknownFileProtocol};

const SubRattProtocol subratt_protocol_load_saved_keys =
    {0, 0, 0, 3, 0, FuriHalSubGhzPresetOok650Async, UnknownFileProtocol};

static const char* subratt_protocol_names[] = {
    [SubRattAttackCAME12bit303] = "CAME 12bit 303MHz",
    [SubRattAttackCAME12bit307] = "CAME 12bit 307MHz",
    [SubRattAttackCAME12bit315] = "CAME 12bit 315MHz",
    [SubRattAttackCAME12bit330] = "CAME 12bit 330MHz",
    [SubRattAttackCAME12bit433] = "CAME 12bit 433MHz",
    [SubRattAttackCAME12bit868] = "CAME 12bit 868MHz",
    [SubRattAttackNICE12bit433] = "NICE 12bit 433MHz",
    [SubRattAttackNICE12bit868] = "NICE 12bit 868MHz",
    [SubRattAttackAnsonic12bit433075] = "Ansonic 12bit 433.07MHz",
    [SubRattAttackAnsonic12bit433] = "Ansonic 12bit 433.92MHz",
    [SubRattAttackAnsonic12bit434] = "Ansonic 12bit 434.07MHz",
    [SubRattAttackHoltek12bitFM433] = "Holtek FM 12bit 433MHz",
    [SubRattAttackHoltek12bitAM433] = "Holtek AM 12bit 433MHz",
    [SubRattAttackHoltek12bitAM315] = "Holtek AM 12bit 315MHz",
    [SubRattAttackHoltek12bitAM868] = "Holtek AM 12bit 868MHz",
    [SubRattAttackHoltek12bitAM915] = "Holtek AM 12bit 915MHz",
    [SubRattAttackChamberlain9bit300] = "Chamberlain 9bit 300MHz",
    [SubRattAttackChamberlain9bit315] = "Chamberlain 9bit 315MHz",
    [SubRattAttackChamberlain9bit318] = "Chamberlain 9bit 318MHz",
    [SubRattAttackChamberlain9bit390] = "Chamberlain 9bit 390MHz",
    [SubRattAttackChamberlain9bit433] = "Chamberlain 9bit 433MHz",
    [SubRattAttackChamberlain8bit300] = "Chamberlain 8bit 300MHz",
    [SubRattAttackChamberlain8bit315] = "Chamberlain 8bit 315MHz",
    [SubRattAttackChamberlain8bit390] = "Chamberlain 8bit 390MHz",
    [SubRattAttackChamberlain7bit300] = "Chamberlain 7bit 300MHz",
    [SubRattAttackChamberlain7bit315] = "Chamberlain 7bit 315MHz",
    [SubRattAttackChamberlain7bit390] = "Chamberlain 7bit 390MHz",
    [SubRattAttackLinear10bit300] = "Linear 10bit 300MHz",
    [SubRattAttackLinear10bit310] = "Linear 10bit 310MHz",
    [SubRattAttackLinearDelta8bit310] = "LinearDelta3 8bit 310MHz",
    [SubRattAttackUNILARM24bit330] = "UNILARM 25bit 330MHz",
    [SubRattAttackUNILARM24bit433] = "UNILARM 25bit 433MHz",
    [SubRattAttackSMC532624bit330] = "SMC5326 25bit 330MHz",
    [SubRattAttackSMC532624bit433] = "SMC5326 25bit 433MHz",
    [SubRattAttackPT226024bit315] = "PT2260 24bit 315MHz",
    [SubRattAttackPT226024bit330] = "PT2260 24bit 330MHz",
    [SubRattAttackPT226024bit390] = "PT2260 24bit 390MHz",
    [SubRattAttackPT226024bit433] = "PT2260 24bit 433MHz",
    [SubRattAttackPT226224bit315] = "PT2262 24bit 315MHz",
    [SubRattAttackPT226224bit418] = "PT2262 24bit 418MHz",
    [SubRattAttackPT226224bit430] = "PT2262 24bit 430MHz",
    [SubRattAttackPT226224bit4305] = "PT2262 24bit 430.5MHz",
    [SubRattAttackPT226224bit433] = "PT2262 24bit 433MHz",
    [SubRattAttackGateTX24bit433] = "GateTX 24bit 433MHz",
    [SubRattAttackMarantec2424bit868] = "Marantec24 24bit 868MHz",
    [SubRattAttackLegrand18bit433] = "Legrand 18bit 433MHz",
    [SubRattAttackClemsa18bit433] = "Clemsa 18bit 433MHz",
    [SubRattAttackBett18bit433] = "BETT 18bit 433MHz",
    [SubRattAttackMegaCode24bit315] = "MegaCode 24bit 315MHz",
    [SubRattAttackHoneywell64bit345] = "Honeywell Sec 64bit 345MHz",
    [SubRattAttackHollarm42bit433] = "Hollarm 42bit 433MHz",
    [SubRattAttackGangQi34bit433] = "GangQi 34bit 433MHz",
    [SubRattAttackMagellan32bit433] = "Magellan 32bit 433MHz",
    [SubRattAttackIntertechnoV332bit433] = "Intertechno_V3 32bit 433MHz",
    [SubRattAttackFeron32bit433] = "Feron 32bit 433MHz",
    [SubRattAttackDoitrand37bit433] = "Doitrand 37bit 433MHz",
    [SubRattAttackMastercode36bit433] = "Mastercode 36bit 433MHz",
    [SubRattAttackX10_32bit310] = "X10 32bit 310MHz",
    [SubRattAttackLoadFile] = "BF existing dump",
    [SubRattAttackLoadSavedKeys] = "Open Saved Keys",
    [SubRattAttackTotalCount] = "Total Count",
};

static const char* subratt_protocol_presets[] = {
    [FuriHalSubGhzPresetIDLE] = "FuriHalSubGhzPresetIDLE",
    [FuriHalSubGhzPresetOok270Async] = "FuriHalSubGhzPresetOok270Async",
    [FuriHalSubGhzPresetOok650Async] = "FuriHalSubGhzPresetOok650Async",
    [FuriHalSubGhzPreset2FSKDev238Async] = "FuriHalSubGhzPreset2FSKDev238Async",
    [FuriHalSubGhzPreset2FSKDev12KAsync] = "FuriHalSubGhzPreset2FSKDev12KAsync",
    [FuriHalSubGhzPreset2FSKDev476Async] = "FuriHalSubGhzPreset2FSKDev476Async",
    [FuriHalSubGhzPresetMSK99_97KbAsync] = "FuriHalSubGhzPresetMSK99_97KbAsync",
    [FuriHalSubGhzPresetGFSK9_99KbAsync] = "FuriHalSubGhzPresetGFSK9_99KbAsync",
};

const SubRattProtocol* subratt_protocol_registry[] = {
    [SubRattAttackCAME12bit303] = &subratt_protocol_came_12bit_303,
    [SubRattAttackCAME12bit307] = &subratt_protocol_came_12bit_307,
    [SubRattAttackCAME12bit315] = &subratt_protocol_came_12bit_315,
    [SubRattAttackCAME12bit330] = &subratt_protocol_came_12bit_330,
    [SubRattAttackCAME12bit433] = &subratt_protocol_came_12bit_433,
    [SubRattAttackCAME12bit868] = &subratt_protocol_came_12bit_868,
    [SubRattAttackNICE12bit433] = &subratt_protocol_nice_12bit_433,
    [SubRattAttackNICE12bit868] = &subratt_protocol_nice_12bit_868,
    [SubRattAttackAnsonic12bit433075] = &subratt_protocol_ansonic_12bit_433075,
    [SubRattAttackAnsonic12bit433] = &subratt_protocol_ansonic_12bit_433,
    [SubRattAttackAnsonic12bit434] = &subratt_protocol_ansonic_12bit_434,
    [SubRattAttackHoltek12bitFM433] = &subratt_protocol_holtek_12bit_433,
    [SubRattAttackHoltek12bitAM433] = &subratt_protocol_holtek_12bit_am_433,
    [SubRattAttackHoltek12bitAM315] = &subratt_protocol_holtek_12bit_am_315,
    [SubRattAttackHoltek12bitAM868] = &subratt_protocol_holtek_12bit_am_868,
    [SubRattAttackHoltek12bitAM915] = &subratt_protocol_holtek_12bit_am_915,
    [SubRattAttackChamberlain9bit300] = &subratt_protocol_chamberlain_9bit_300,
    [SubRattAttackChamberlain9bit315] = &subratt_protocol_chamberlain_9bit_315,
    [SubRattAttackChamberlain9bit318] = &subratt_protocol_chamberlain_9bit_318,
    [SubRattAttackChamberlain9bit390] = &subratt_protocol_chamberlain_9bit_390,
    [SubRattAttackChamberlain9bit433] = &subratt_protocol_chamberlain_9bit_433,
    [SubRattAttackChamberlain8bit300] = &subratt_protocol_chamberlain_8bit_300,
    [SubRattAttackChamberlain8bit315] = &subratt_protocol_chamberlain_8bit_315,
    [SubRattAttackChamberlain8bit390] = &subratt_protocol_chamberlain_8bit_390,
    [SubRattAttackChamberlain7bit300] = &subratt_protocol_chamberlain_7bit_300,
    [SubRattAttackChamberlain7bit315] = &subratt_protocol_chamberlain_7bit_315,
    [SubRattAttackChamberlain7bit390] = &subratt_protocol_chamberlain_7bit_390,
    [SubRattAttackLinear10bit300] = &subratt_protocol_linear_10bit_300,
    [SubRattAttackLinear10bit310] = &subratt_protocol_linear_10bit_310,
    [SubRattAttackLinearDelta8bit310] = &subratt_protocol_linear_delta_8bit_310,
    [SubRattAttackUNILARM24bit330] = &subratt_protocol_unilarm_24bit_330,
    [SubRattAttackUNILARM24bit433] = &subratt_protocol_unilarm_24bit_433,
    [SubRattAttackSMC532624bit330] = &subratt_protocol_smc5326_24bit_330,
    [SubRattAttackSMC532624bit433] = &subratt_protocol_smc5326_24bit_433,
    [SubRattAttackPT226024bit315] = &subratt_protocol_pt2260_24bit_315,
    [SubRattAttackPT226024bit330] = &subratt_protocol_pt2260_24bit_330,
    [SubRattAttackPT226024bit390] = &subratt_protocol_pt2260_24bit_390,
    [SubRattAttackPT226024bit433] = &subratt_protocol_pt2260_24bit_433,
    [SubRattAttackPT226224bit315] = &subratt_protocol_pt2262_24bit_315,
    [SubRattAttackPT226224bit418] = &subratt_protocol_pt2262_24bit_418,
    [SubRattAttackPT226224bit430] = &subratt_protocol_pt2262_24bit_430,
    [SubRattAttackPT226224bit4305] = &subratt_protocol_pt2262_24bit_430_5,
    [SubRattAttackPT226224bit433] = &subratt_protocol_pt2262_24bit_433,
    [SubRattAttackGateTX24bit433] = &subratt_protocol_gate_tx_24bit_433,
    [SubRattAttackMarantec2424bit868] = &subratt_protocol_marantec24_24bit_868,
    [SubRattAttackLegrand18bit433] = &subratt_protocol_legrand_18bit_433,
    [SubRattAttackClemsa18bit433] = &subratt_protocol_clemsa_18bit_433,
    [SubRattAttackBett18bit433] = &subratt_protocol_bett_18bit_433,
    [SubRattAttackMegaCode24bit315] = &subratt_protocol_megacode_24bit_315,
    [SubRattAttackHoneywell64bit345] = &subratt_protocol_honeywell_64bit_345,
    [SubRattAttackHollarm42bit433] = &subratt_protocol_hollarm_42bit_433,
    [SubRattAttackGangQi34bit433] = &subratt_protocol_gangqi_34bit_433,
    [SubRattAttackMagellan32bit433] = &subratt_protocol_magellan_32bit_433,
    [SubRattAttackIntertechnoV332bit433] = &subratt_protocol_intertechno_v3_32bit_433,
    [SubRattAttackFeron32bit433] = &subratt_protocol_feron_32bit_433,
    [SubRattAttackDoitrand37bit433] = &subratt_protocol_doitrand_37bit_433,
    [SubRattAttackMastercode36bit433] = &subratt_protocol_mastercode_36bit_433,
    [SubRattAttackX10_32bit310] = &subratt_protocol_x10_32bit_310,
    [SubRattAttackLoadFile] = &subratt_protocol_load_file,
    [SubRattAttackLoadSavedKeys] = &subratt_protocol_load_saved_keys};

static const SubRattAttacks came_12bit_attacks[] = {
    SubRattAttackCAME12bit303,
    SubRattAttackCAME12bit307,
    SubRattAttackCAME12bit315,
    SubRattAttackCAME12bit330,
    SubRattAttackCAME12bit433,
    SubRattAttackCAME12bit868,
};
static const SubRattTypeGroup came_types[] = {
    {.name = "12bit", .attacks = came_12bit_attacks, .attack_count = 6},
};

static const SubRattAttacks nice_12bit_attacks[] = {
    SubRattAttackNICE12bit433,
    SubRattAttackNICE12bit868,
};
static const SubRattTypeGroup nice_types[] = {
    {.name = "12bit", .attacks = nice_12bit_attacks, .attack_count = 2},
};

static const SubRattAttacks ansonic_12bit_attacks[] = {
    SubRattAttackAnsonic12bit433075,
    SubRattAttackAnsonic12bit433,
    SubRattAttackAnsonic12bit434,
};
static const SubRattTypeGroup ansonic_types[] = {
    {.name = "12bit", .attacks = ansonic_12bit_attacks, .attack_count = 3},
};

static const SubRattAttacks holtek_fm_attacks[] = {
    SubRattAttackHoltek12bitFM433,
};
static const SubRattAttacks holtek_am_attacks[] = {
    SubRattAttackHoltek12bitAM433,
    SubRattAttackHoltek12bitAM315,
    SubRattAttackHoltek12bitAM868,
    SubRattAttackHoltek12bitAM915,
};
static const SubRattTypeGroup holtek_types[] = {
    {.name = "FM 12bit", .attacks = holtek_fm_attacks, .attack_count = 1},
    {.name = "AM 12bit", .attacks = holtek_am_attacks, .attack_count = 4},
};

static const SubRattAttacks chamberlain_9bit_attacks[] = {
    SubRattAttackChamberlain9bit300,
    SubRattAttackChamberlain9bit315,
    SubRattAttackChamberlain9bit318,
    SubRattAttackChamberlain9bit390,
    SubRattAttackChamberlain9bit433,
};
static const SubRattAttacks chamberlain_8bit_attacks[] = {
    SubRattAttackChamberlain8bit300,
    SubRattAttackChamberlain8bit315,
    SubRattAttackChamberlain8bit390,
};
static const SubRattAttacks chamberlain_7bit_attacks[] = {
    SubRattAttackChamberlain7bit300,
    SubRattAttackChamberlain7bit315,
    SubRattAttackChamberlain7bit390,
};
static const SubRattTypeGroup chamberlain_types[] = {
    {.name = "9bit", .attacks = chamberlain_9bit_attacks, .attack_count = 5},
    {.name = "8bit", .attacks = chamberlain_8bit_attacks, .attack_count = 3},
    {.name = "7bit", .attacks = chamberlain_7bit_attacks, .attack_count = 3},
};

static const SubRattAttacks linear_10bit_attacks[] = {
    SubRattAttackLinear10bit300,
    SubRattAttackLinear10bit310,
};
static const SubRattAttacks linear_delta_attacks[] = {
    SubRattAttackLinearDelta8bit310,
};
static const SubRattTypeGroup linear_types[] = {
    {.name = "10bit", .attacks = linear_10bit_attacks, .attack_count = 2},
    {.name = "Delta 8bit", .attacks = linear_delta_attacks, .attack_count = 1},
};

static const SubRattAttacks unilarm_25bit_attacks[] = {
    SubRattAttackUNILARM24bit330,
    SubRattAttackUNILARM24bit433,
};
static const SubRattTypeGroup unilarm_types[] = {
    {.name = "25bit", .attacks = unilarm_25bit_attacks, .attack_count = 2},
};

static const SubRattAttacks smc5326_25bit_attacks[] = {
    SubRattAttackSMC532624bit330,
    SubRattAttackSMC532624bit433,
};
static const SubRattTypeGroup smc5326_types[] = {
    {.name = "25bit", .attacks = smc5326_25bit_attacks, .attack_count = 2},
};

static const SubRattAttacks pt2260_24bit_attacks[] = {
    SubRattAttackPT226024bit315,
    SubRattAttackPT226024bit330,
    SubRattAttackPT226024bit390,
    SubRattAttackPT226024bit433,
};
static const SubRattTypeGroup pt2260_types[] = {
    {.name = "24bit", .attacks = pt2260_24bit_attacks, .attack_count = 4},
};

static const SubRattAttacks pt2262_24bit_attacks[] = {
    SubRattAttackPT226224bit315,
    SubRattAttackPT226224bit418,
    SubRattAttackPT226224bit430,
    SubRattAttackPT226224bit4305,
    SubRattAttackPT226224bit433,
};
static const SubRattTypeGroup pt2262_types[] = {
    {.name = "24bit", .attacks = pt2262_24bit_attacks, .attack_count = 5},
};

static const SubRattAttacks load_file_attacks[] = {
    SubRattAttackLoadFile,
};
static const SubRattTypeGroup load_file_types[] = {
    {.name = "Load", .attacks = load_file_attacks, .attack_count = 1},
};

static const SubRattAttacks load_saved_keys_attacks[] = {
    SubRattAttackLoadSavedKeys,
};
static const SubRattTypeGroup load_saved_keys_types[] = {
    {.name = "Load", .attacks = load_saved_keys_attacks, .attack_count = 1},
};

static const SubRattAttacks gate_tx_24bit_attacks[] = {
    SubRattAttackGateTX24bit433,
};
static const SubRattTypeGroup gate_tx_types[] = {
    {.name = "24bit", .attacks = gate_tx_24bit_attacks, .attack_count = 1},
};

static const SubRattAttacks marantec24_24bit_attacks[] = {
    SubRattAttackMarantec2424bit868,
};
static const SubRattTypeGroup marantec24_types[] = {
    {.name = "24bit", .attacks = marantec24_24bit_attacks, .attack_count = 1},
};

static const SubRattAttacks legrand_18bit_attacks[] = {
    SubRattAttackLegrand18bit433,
};
static const SubRattTypeGroup legrand_types[] = {
    {.name = "18bit", .attacks = legrand_18bit_attacks, .attack_count = 1},
};

static const SubRattAttacks clemsa_18bit_attacks[] = {
    SubRattAttackClemsa18bit433,
};
static const SubRattTypeGroup clemsa_types[] = {
    {.name = "18bit", .attacks = clemsa_18bit_attacks, .attack_count = 1},
};

static const SubRattAttacks bett_18bit_attacks[] = {
    SubRattAttackBett18bit433,
};
static const SubRattTypeGroup bett_types[] = {
    {.name = "18bit", .attacks = bett_18bit_attacks, .attack_count = 1},
};

static const SubRattAttacks megacode_24bit_attacks[] = {
    SubRattAttackMegaCode24bit315,
};
static const SubRattTypeGroup megacode_types[] = {
    {.name = "24bit", .attacks = megacode_24bit_attacks, .attack_count = 1},
};

static const SubRattAttacks honeywell_64bit_attacks[] = {
    SubRattAttackHoneywell64bit345,
};
static const SubRattTypeGroup honeywell_types[] = {
    {.name = "64bit", .attacks = honeywell_64bit_attacks, .attack_count = 1},
};

static const SubRattAttacks hollarm_42bit_attacks[] = {
    SubRattAttackHollarm42bit433,
};
static const SubRattTypeGroup hollarm_types[] = {
    {.name = "42bit", .attacks = hollarm_42bit_attacks, .attack_count = 1},
};

static const SubRattAttacks gangqi_34bit_attacks[] = {
    SubRattAttackGangQi34bit433,
};
static const SubRattTypeGroup gangqi_types[] = {
    {.name = "34bit", .attacks = gangqi_34bit_attacks, .attack_count = 1},
};

static const SubRattAttacks magellan_32bit_attacks[] = {
    SubRattAttackMagellan32bit433,
};
static const SubRattTypeGroup magellan_types[] = {
    {.name = "32bit", .attacks = magellan_32bit_attacks, .attack_count = 1},
};

static const SubRattAttacks intertechno_v3_32bit_attacks[] = {
    SubRattAttackIntertechnoV332bit433,
};
static const SubRattTypeGroup intertechno_v3_types[] = {
    {.name = "32bit", .attacks = intertechno_v3_32bit_attacks, .attack_count = 1},
};

static const SubRattAttacks feron_32bit_attacks[] = {
    SubRattAttackFeron32bit433,
};
static const SubRattTypeGroup feron_types[] = {
    {.name = "32bit", .attacks = feron_32bit_attacks, .attack_count = 1},
};

static const SubRattAttacks doitrand_37bit_attacks[] = {
    SubRattAttackDoitrand37bit433,
};
static const SubRattTypeGroup doitrand_types[] = {
    {.name = "37bit", .attacks = doitrand_37bit_attacks, .attack_count = 1},
};

static const SubRattAttacks mastercode_36bit_attacks[] = {
    SubRattAttackMastercode36bit433,
};
static const SubRattTypeGroup mastercode_types[] = {
    {.name = "36bit", .attacks = mastercode_36bit_attacks, .attack_count = 1},
};

static const SubRattAttacks x10_32bit_attacks[] = {
    SubRattAttackX10_32bit310,
};
static const SubRattTypeGroup x10_types[] = {
    {.name = "32bit", .attacks = x10_32bit_attacks, .attack_count = 1},
};

static const SubRattBrandGroup subratt_brand_groups[] = {
    [SubRattBrandCAME] = {"CAME", came_types, 1},
    [SubRattBrandNICE] = {"NICE", nice_types, 1},
    [SubRattBrandAnsonic] = {"Ansonic", ansonic_types, 1},
    [SubRattBrandHoltek] = {"Holtek", holtek_types, 2},
    [SubRattBrandChamberlain] = {"Chamberlain", chamberlain_types, 3},
    [SubRattBrandLinear] = {"Linear", linear_types, 2},
    [SubRattBrandUNILARM] = {"UNILARM", unilarm_types, 1},
    [SubRattBrandSMC5326] = {"SMC5326", smc5326_types, 1},
    [SubRattBrandPT2260] = {"PT2260", pt2260_types, 1},
    [SubRattBrandPT2262] = {"PT2262", pt2262_types, 1},
    [SubRattBrandGateTX] = {"GateTX", gate_tx_types, 1},
    [SubRattBrandMarantec24] = {"Marantec24", marantec24_types, 1},
    [SubRattBrandLegrand] = {"Legrand", legrand_types, 1},
    [SubRattBrandClemsa] = {"Clemsa", clemsa_types, 1},
    [SubRattBrandBett] = {"BETT", bett_types, 1},
    [SubRattBrandMegaCode] = {"MegaCode", megacode_types, 1},
    [SubRattBrandHoneywell] = {"Honeywell", honeywell_types, 1},
    [SubRattBrandHollarm] = {"Hollarm", hollarm_types, 1},
    [SubRattBrandGangQi] = {"GangQi", gangqi_types, 1},
    [SubRattBrandMagellan] = {"Magellan", magellan_types, 1},
    [SubRattBrandIntertechnoV3] = {"Intertechno_V3", intertechno_v3_types, 1},
    [SubRattBrandFeron] = {"Feron", feron_types, 1},
    [SubRattBrandDoitrand] = {"Doitrand", doitrand_types, 1},
    [SubRattBrandMastercode] = {"Mastercode", mastercode_types, 1},
    [SubRattBrandX10] = {"X10", x10_types, 1},
    [SubRattBrandLoadFile] = {"BF existing dump", load_file_types, 1},
    [SubRattBrandLoadSavedKeys] = {"Open Saved Keys", load_saved_keys_types, 1},
};

static const char* subratt_protocol_freq_names[] = {
    [SubRattAttackCAME12bit303] = "303MHz",
    [SubRattAttackCAME12bit307] = "307MHz",
    [SubRattAttackCAME12bit315] = "315MHz",
    [SubRattAttackCAME12bit330] = "330MHz",
    [SubRattAttackCAME12bit433] = "433MHz",
    [SubRattAttackCAME12bit868] = "868MHz",
    [SubRattAttackNICE12bit433] = "433MHz",
    [SubRattAttackNICE12bit868] = "868MHz",
    [SubRattAttackAnsonic12bit433075] = "433.07MHz",
    [SubRattAttackAnsonic12bit433] = "433.92MHz",
    [SubRattAttackAnsonic12bit434] = "434.07MHz",
    [SubRattAttackHoltek12bitFM433] = "433MHz",
    [SubRattAttackHoltek12bitAM433] = "433MHz",
    [SubRattAttackHoltek12bitAM315] = "315MHz",
    [SubRattAttackHoltek12bitAM868] = "868MHz",
    [SubRattAttackHoltek12bitAM915] = "915MHz",
    [SubRattAttackChamberlain9bit300] = "300MHz",
    [SubRattAttackChamberlain9bit315] = "315MHz",
    [SubRattAttackChamberlain9bit318] = "318MHz",
    [SubRattAttackChamberlain9bit390] = "390MHz",
    [SubRattAttackChamberlain9bit433] = "433MHz",
    [SubRattAttackChamberlain8bit300] = "300MHz",
    [SubRattAttackChamberlain8bit315] = "315MHz",
    [SubRattAttackChamberlain8bit390] = "390MHz",
    [SubRattAttackChamberlain7bit300] = "300MHz",
    [SubRattAttackChamberlain7bit315] = "315MHz",
    [SubRattAttackChamberlain7bit390] = "390MHz",
    [SubRattAttackLinear10bit300] = "300MHz",
    [SubRattAttackLinear10bit310] = "310MHz",
    [SubRattAttackLinearDelta8bit310] = "310MHz",
    [SubRattAttackUNILARM24bit330] = "330MHz",
    [SubRattAttackUNILARM24bit433] = "433MHz",
    [SubRattAttackSMC532624bit330] = "330MHz",
    [SubRattAttackSMC532624bit433] = "433MHz",
    [SubRattAttackPT226024bit315] = "315MHz",
    [SubRattAttackPT226024bit330] = "330MHz",
    [SubRattAttackPT226024bit390] = "390MHz",
    [SubRattAttackPT226024bit433] = "433MHz",
    [SubRattAttackPT226224bit315] = "315MHz",
    [SubRattAttackPT226224bit418] = "418MHz",
    [SubRattAttackPT226224bit430] = "430MHz",
    [SubRattAttackPT226224bit4305] = "430.5MHz",
    [SubRattAttackPT226224bit433] = "433MHz",
    [SubRattAttackGateTX24bit433] = "433MHz",
    [SubRattAttackMarantec2424bit868] = "868MHz",
    [SubRattAttackLegrand18bit433] = "433MHz",
    [SubRattAttackClemsa18bit433] = "433MHz",
    [SubRattAttackBett18bit433] = "433MHz",
    [SubRattAttackMegaCode24bit315] = "315MHz",
    [SubRattAttackHoneywell64bit345] = "345MHz",
    [SubRattAttackHollarm42bit433] = "433MHz",
    [SubRattAttackGangQi34bit433] = "433MHz",
    [SubRattAttackMagellan32bit433] = "433MHz",
    [SubRattAttackIntertechnoV332bit433] = "433MHz",
    [SubRattAttackFeron32bit433] = "433MHz",
    [SubRattAttackDoitrand37bit433] = "433MHz",
    [SubRattAttackMastercode36bit433] = "433MHz",
    [SubRattAttackX10_32bit310] = "310MHz",
    [SubRattAttackLoadFile] = "BF existing dump",
    [SubRattAttackLoadSavedKeys] = "Open Saved Keys",
};

static const char* subratt_protocol_file_types[] = {
    [CAMEFileProtocol] = "CAME",
    [NICEFileProtocol] = "Nice FLO",
    [ChamberlainFileProtocol] = "Cham_Code",
    [LinearFileProtocol] = "Linear",
    [LinearDeltaFileProtocol] = "LinearDelta3",
    [PrincetonFileProtocol] = "Princeton",
    [RAWFileProtocol] = "RAW",
    [BETTFileProtocol] = "BETT",
    [ClemsaFileProtocol] = "Clemsa",
    [DoitrandFileProtocol] = "Doitrand",
    [GateTXFileProtocol] = "GateTX",
    [MagellanFileProtocol] = "Magellan",
    [IntertechnoV3FileProtocol] = "Intertechno_V3",
    [AnsonicFileProtocol] = "Ansonic",
    [SMC5326FileProtocol] = "SMC5326",
    [UNILARMFileProtocol] = "SMC5326",
    [PT2260FileProtocol] = "Princeton",
    [PT2262FileProtocol] = "Princeton",

    [HoneywellFileProtocol] = "Honeywell Sec",
    [HoltekFileProtocol] = "Holtek_HT12X",
    [LegrandFileProtocol] = "Legrand",
    [HollarmFileProtocol] = "Hollarm",
    [GangQiFileProtocol] = "GangQi",
    [Marantec24FileProtocol] = "Marantec24",
    [FeronFileProtocol] = "Feron",
    [MegaCodeFileProtocol] = "MegaCode",
    [MastercodeFileProtocol] = "Mastercode",
    [X10FileProtocol] = "X10",
    [UnknownFileProtocol] = "Unknown"};

static const char* subratt_key_file_start_no_tail =
    "Filetype: Flipper SubGhz Key File\nVersion: 1\nFrequency: %u\nPreset: %s\nProtocol: %s\nBit: %d\nKey: %s\n";
static const char* subratt_key_file_start_with_tail =
    "Filetype: Flipper SubGhz Key File\nVersion: 1\nFrequency: %u\nPreset: %s\nProtocol: %s\nBit: %d\nKey: %s\nTE: %d\n";
static const char* subratt_key_small_no_tail = "Bit: %d\nKey: %s\nRepeat: %d\n";

static const char* subratt_key_small_with_tail = "Bit: %d\nKey: %s\nTE: %d\nRepeat: %d\n";

const uint8_t lut_uni_alarm_smsc[] = {0x00, 0x02, 0x03};
const uint8_t lut_pt2260[] = {0x00, 0x01, 0x03};
const uint8_t lut_pt2262[] = {0x00, 0x01, 0x03};

const uint64_t gate_smsc = 0x01D5;

const uint64_t gate_pt2260 = 0x03;

const uint64_t gate_uni_alarm = 3 << 7;

const char* subratt_protocol_name(SubRattAttacks index) {
    return subratt_protocol_names[index];
}

const SubRattProtocol* subratt_protocol(SubRattAttacks index) {
    return subratt_protocol_registry[index];
}

uint8_t subratt_protocol_repeats_count(SubRattAttacks index) {
    return subratt_protocol_registry[index]->repeat;
}

const char* subratt_protocol_preset(FuriHalSubGhzPreset preset) {
    return subratt_protocol_presets[preset];
}

const char* subratt_protocol_file(SubRattFileProtocol protocol) {
    return subratt_protocol_file_types[protocol];
}

FuriHalSubGhzPreset subratt_protocol_convert_preset(FuriString* preset_name) {
    for(size_t i = FuriHalSubGhzPresetIDLE; i < FuriHalSubGhzPresetCustom; i++) {
        if(furi_string_cmp_str(preset_name, subratt_protocol_presets[i]) == 0) {
            return i;
        }
    }

    return FuriHalSubGhzPresetIDLE;
}

SubRattFileProtocol subratt_protocol_file_protocol_name(FuriString* name) {
    for(size_t i = CAMEFileProtocol; i < TotalFileProtocol - 1; i++) {
        if(furi_string_cmp_str(name, subratt_protocol_file_types[i]) == 0) {
            return i;
        }
    }

    return UnknownFileProtocol;
}

void subratt_protocol_create_candidate_for_existing_file(
    FuriString* candidate,
    uint64_t step,
    size_t bit_index,
    uint64_t file_key,
    bool two_bytes) {
    uint8_t p[8] = {0};
    for(int i = 0; i < 8; i++) {
        p[i] = (uint8_t)(file_key >> 8 * (7 - i)) & 0xFF;
    }
    uint8_t low_byte = step & (0xff);
    uint8_t high_byte = (step >> 8) & 0xff;

    size_t size = sizeof(uint64_t);
    for(size_t i = 0; i < size; i++) {
        if(i == bit_index - 1 && two_bytes) {
            furi_string_cat_printf(candidate, "%02X %02X", high_byte, low_byte);
            i++;
        } else if(i == bit_index) {
            furi_string_cat_printf(candidate, "%02X", low_byte);
        } else if(p[i] != 0) {
            furi_string_cat_printf(candidate, "%02X", p[i]);
        } else {
            furi_string_cat_printf(candidate, "%s", "00");
        }

        if(i < size - 1) {
            furi_string_push_back(candidate, ' ');
        }
    }

#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "file candidate: %s, step: %lld", furi_string_get_cstr(candidate), step);
#endif
}

void subratt_protocol_create_candidate_for_default(
    FuriString* candidate,
    SubRattFileProtocol file,
    uint64_t step,
    uint8_t opencode) {
    uint8_t p[8] = {0};
    uint64_t total = 0;

    if(file == SMC5326FileProtocol) {
        for(size_t j = 0; j < 8; j++) {
            total |= lut_uni_alarm_smsc[step % 3] << (2 * j);
            double sub_step = (double)step / 3;
            step = (uint64_t)floor(sub_step);
        }
        total <<= 9;
        total |= gate_smsc;

        for(int i = 0; i < 8; i++) {
            p[i] = (uint8_t)(total >> 8 * (7 - i)) & 0xFF;
        }
    } else if(file == UNILARMFileProtocol) {
        for(size_t j = 0; j < 8; j++) {
            total |= lut_uni_alarm_smsc[step % 3] << (2 * j);
            double sub_step = (double)step / 3;
            step = (uint64_t)floor(sub_step);
        }
        total <<= 9;
        total |= gate_uni_alarm;

        for(int i = 0; i < 8; i++) {
            p[i] = (uint8_t)(total >> 8 * (7 - i)) & 0xFF;
        }
    } else if(file == PT2260FileProtocol) {
        for(size_t j = 0; j < 8; j++) {
            total |= lut_pt2260[step % 3] << (2 * j);
            double sub_step = (double)step / 3;
            step = (uint64_t)floor(sub_step);
        }
        total <<= 8;
        total |= gate_pt2260;

        for(int i = 0; i < 8; i++) {
            p[i] = (uint8_t)(total >> 8 * (7 - i)) & 0xFF;
        }
    } else if(file == PT2262FileProtocol) {
        uint64_t gate_pt2262 = 0x03;
        uint8_t opencode_var = opencode;
        if(opencode_var == 0) {
            gate_pt2262 = 0x03;
        }
        if(opencode_var == 1) {
            gate_pt2262 = 0x0C;
        }
        if(opencode_var == 2) {
            gate_pt2262 = 0x30;
        }
        if(opencode_var == 3) {
            gate_pt2262 = 0xC0;
        }
        if(opencode_var == 4) {
            gate_pt2262 = 0xF0;
        }
        if(opencode_var == 5) {
            gate_pt2262 = 0x10;
        }
        if(opencode_var == 6) {
            gate_pt2262 = 0x04;
        }
        if(opencode_var == 7) {
            gate_pt2262 = 0x40;
        }
        if(opencode_var == 8) {
            gate_pt2262 = 0xC3;
        }
        for(size_t j = 0; j < 8; j++) {
            total |= lut_pt2262[step % 3] << (2 * j);
            double sub_step = (double)step / 3;
            step = (uint64_t)floor(sub_step);
        }
        total <<= 8;
        total |= gate_pt2262;

        for(int i = 0; i < 8; i++) {
            p[i] = (uint8_t)(total >> 8 * (7 - i)) & 0xFFU;
        }
    } else if(file == X10FileProtocol) {

        uint8_t byte_hi = (uint8_t)((step >> 8) & 0xFF);
        uint8_t byte_lo = (uint8_t)(step & 0xFF);
        total = ((uint64_t)byte_hi << 24) | ((uint64_t)(~byte_hi & 0xFF) << 16) |
                ((uint64_t)byte_lo << 8) | (uint64_t)(~byte_lo & 0xFF);

        for(int i = 0; i < 8; i++) {
            p[i] = (uint8_t)(total >> 8 * (7 - i)) & 0xFF;
        }
    } else {
        for(int i = 0; i < 8; i++) {
            p[i] = (uint8_t)(step >> 8 * (7 - i)) & 0xFF;
        }
    }

    size_t size = sizeof(uint64_t);
    for(size_t i = 0; i < size; i++) {
        if(p[i] != 0) {
            furi_string_cat_printf(candidate, "%02X", p[i]);
        } else {
            furi_string_cat_printf(candidate, "%s", "00");
        }

        if(i < size - 1) {
            furi_string_push_back(candidate, ' ');
        }
    }

#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "candidate: %s, step: %lld", furi_string_get_cstr(candidate), step);
#endif
}

void subratt_protocol_default_payload(
    Stream* stream,
    SubRattFileProtocol file,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t repeat,
    uint8_t opencode) {
    FuriString* candidate = furi_string_alloc();
    subratt_protocol_create_candidate_for_default(candidate, file, step, opencode);

#ifdef FURI_DEBUG
    FURI_LOG_D(
        TAG,
        "candidate: %s, step: %lld, repeat: %d, te: %s",
        furi_string_get_cstr(candidate),
        step,
        repeat,
        te ? "true" : "false");
#endif
    stream_clean(stream);
    if(te) {
        stream_write_format(
            stream,
            subratt_key_small_with_tail,
            bits,
            furi_string_get_cstr(candidate),
            te,
            repeat);
    } else {
        stream_write_format(
            stream, subratt_key_small_no_tail, bits, furi_string_get_cstr(candidate), repeat);
    }

    furi_string_free(candidate);
}

void subratt_protocol_file_payload(
    Stream* stream,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t repeat,
    uint8_t bit_index,
    uint64_t file_key,
    bool two_bytes) {
    FuriString* candidate = furi_string_alloc();
    subratt_protocol_create_candidate_for_existing_file(
        candidate, step, bit_index, file_key, two_bytes);

#ifdef FURI_DEBUG
    FURI_LOG_D(
        TAG,
        "candidate: %s, step: %lld, repeat: %d, te: %s",
        furi_string_get_cstr(candidate),
        step,
        repeat,
        te ? "true" : "false");
#endif
    stream_clean(stream);

    if(te) {
        stream_write_format(
            stream,
            subratt_key_small_with_tail,
            bits,
            furi_string_get_cstr(candidate),
            te,
            repeat);
    } else {
        stream_write_format(
            stream, subratt_key_small_no_tail, bits, furi_string_get_cstr(candidate), repeat);
    }

    furi_string_free(candidate);
}

void subratt_protocol_default_generate_file(
    Stream* stream,
    uint32_t frequency,
    FuriHalSubGhzPreset preset,
    SubRattFileProtocol file,
    uint64_t step,
    uint8_t bits,
    uint32_t te,
    uint8_t opencode) {
    FuriString* candidate = furi_string_alloc();
    subratt_protocol_create_candidate_for_default(candidate, file, step, opencode);

#ifdef FURI_DEBUG
    FURI_LOG_D(TAG, "candidate: %s, step: %lld", furi_string_get_cstr(candidate), step);
#endif
    stream_clean(stream);

    if(te) {
        stream_write_format(
            stream,
            subratt_key_file_start_with_tail,
            frequency,
            subratt_protocol_preset(preset),
            subratt_protocol_file(file),
            bits,
            furi_string_get_cstr(candidate),
            te);
    } else {
        stream_write_format(
            stream,
            subratt_key_file_start_no_tail,
            frequency,
            subratt_protocol_preset(preset),
            subratt_protocol_file(file),
            bits,
            furi_string_get_cstr(candidate));
    }

    furi_string_free(candidate);
}

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
    bool two_bytes) {
    FuriString* candidate = furi_string_alloc();
    subratt_protocol_create_candidate_for_existing_file(
        candidate, step, bit_index, file_key, two_bytes);

    stream_clean(stream);

    if(te) {
        stream_write_format(
            stream,
            subratt_key_file_start_with_tail,
            frequency,
            subratt_protocol_preset(preset),
            subratt_protocol_file(file),
            bits,
            furi_string_get_cstr(candidate),
            te);
    } else {
        stream_write_format(
            stream,
            subratt_key_file_start_no_tail,
            frequency,
            subratt_protocol_preset(preset),
            subratt_protocol_file(file),
            bits,
            furi_string_get_cstr(candidate));
    }

    furi_string_free(candidate);
}

const SubRattBrandGroup* subratt_brand_group(SubRattBrand brand) {
    furi_assert(brand < SubRattBrandCount);
    return &subratt_brand_groups[brand];
}

const char* subratt_protocol_freq_name(SubRattAttacks index) {
    return subratt_protocol_freq_names[index];
}

void subratt_protocol_find_brand_type(
    SubRattAttacks attack,
    uint8_t* brand,
    uint8_t* type,
    uint8_t* freq_idx) {
    for(uint8_t b = 0; b < SubRattBrandCount; b++) {
        const SubRattBrandGroup* bg = &subratt_brand_groups[b];
        for(uint8_t t = 0; t < bg->type_count; t++) {
            const SubRattTypeGroup* tg = &bg->types[t];
            for(uint8_t f = 0; f < tg->attack_count; f++) {
                if(tg->attacks[f] == attack) {
                    *brand = b;
                    *type = t;
                    *freq_idx = f;
                    return;
                }
            }
        }
    }

    *brand = 0;
    *type = 0;
    *freq_idx = 0;
}

const char* subratt_protocol_title(SubRattAttacks attack) {
    uint8_t brand = 0, type = 0, freq_idx = 0;
    subratt_protocol_find_brand_type(attack, &brand, &type, &freq_idx);
    (void)freq_idx;
    const SubRattBrandGroup* bg = subratt_brand_group(brand);
    if(bg->type_count == 1) {
        return bg->name;
    }
    static char title_buf[32];
    snprintf(title_buf, sizeof(title_buf), "%s > %s", bg->name, bg->types[type].name);
    return title_buf;
}

uint64_t
    subratt_protocol_calc_max_value(SubRattAttacks attack_type, uint8_t bits, bool two_bytes) {
    uint64_t max_value;
    if(attack_type == SubRattAttackLoadFile) {
        max_value = two_bytes ? 0xFFFF : 0xFF;
    } else if(
        attack_type == SubRattAttackSMC532624bit330 ||
        attack_type == SubRattAttackSMC532624bit433 ||
        attack_type == SubRattAttackUNILARM24bit330 ||
        attack_type == SubRattAttackUNILARM24bit433 ||
        attack_type == SubRattAttackPT226024bit315 ||
        attack_type == SubRattAttackPT226024bit330 ||
        attack_type == SubRattAttackPT226024bit390 ||
        attack_type == SubRattAttackPT226024bit433 ||
        attack_type == SubRattAttackPT226224bit315 ||
        attack_type == SubRattAttackPT226224bit418 ||
        attack_type == SubRattAttackPT226224bit430 ||
        attack_type == SubRattAttackPT226224bit4305 ||
        attack_type == SubRattAttackPT226224bit433) {
        max_value = 6561;
    } else if(attack_type == SubRattAttackX10_32bit310) {

        max_value = 0xFFFFULL;
    } else if(bits > 25) {

        max_value = (bits >= 64) ? UINT64_MAX : ((1ULL << bits) - 1ULL);
    } else {
        FuriString* max_value_s;
        max_value_s = furi_string_alloc();
        for(uint8_t i = 0; i < bits; i++) {
            furi_string_cat_printf(max_value_s, "1");
        }
        max_value = (uint64_t)strtol(furi_string_get_cstr(max_value_s), NULL, 2);
        furi_string_free(max_value_s);
    }

    return max_value;
}

uint64_t subratt_protocol_random_step(SubRattFileProtocol file, uint8_t bits) {

    uint8_t effective_bits = (file == X10FileProtocol) ? 16 : bits;

    uint64_t r = ((uint64_t)furi_hal_random_get() << 32) | (uint64_t)furi_hal_random_get();

    uint64_t mask = (effective_bits >= 64) ? UINT64_MAX : ((1ULL << effective_bits) - 1ULL);
    return r & mask;
}
