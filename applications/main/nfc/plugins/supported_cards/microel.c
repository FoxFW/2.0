#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#include <bit_lib.h>

#define TAG "Microel"

#define KEY_LENGTH 6
#define UID_LENGTH 4

typedef struct {
    uint64_t a;
    uint64_t b;
} MfClassicKeyPair;

static MfClassicKeyPair microel_1k_keys[] = {
    {.a = 0x000000000000, .b = 0x000000000000},
    {.a = 0x000000000000, .b = 0x000000000000},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff},
};

const uint8_t verify_sector = 1;

void calculateSumHex(const uint8_t* uid, size_t uidSize, uint8_t sumHex[]) {
    const uint8_t xorKey[] = {0x01, 0x92, 0xA7, 0x75, 0x2B, 0xF9};
    int sum = 0;

    for(size_t i = 0; i < uidSize; i++) {
        sum += uid[i];
    }

    int sumTwoDigits = sum % 256;

    if(sumTwoDigits % 2 == 1) {
        sumTwoDigits += 2;
    }

    for(size_t i = 0; i < sizeof(xorKey); i++) {
        sumHex[i] = sumTwoDigits ^ xorKey[i];
    }
}

void generateKeyA(const uint8_t* uid, uint8_t uidSize, uint8_t keyA[]) {
    uint8_t sumHex[6];
    calculateSumHex(uid, uidSize, sumHex);
    uint8_t firstCharacter = (sumHex[0] >> 4) & 0xF;

    if(firstCharacter == 0x2 || firstCharacter == 0x3 || firstCharacter == 0xA ||
       firstCharacter == 0xB) {

        for(size_t i = 0; i < sizeof(sumHex); i++) {
            keyA[i] = 0x40 ^ sumHex[i];
        }
    } else if(
        firstCharacter == 0x6 || firstCharacter == 0x7 || firstCharacter == 0xE ||
        firstCharacter == 0xF) {

        for(size_t i = 0; i < sizeof(sumHex); i++) {
            keyA[i] = 0xC0 ^ sumHex[i];
        }
    } else {

        for(size_t i = 0; i < sizeof(sumHex); i++) {
            keyA[i] = sumHex[i];
        }
    }
}

void generateKeyB(uint8_t keyA[], size_t keyASize, uint8_t keyB[]) {
    for(size_t i = 0; i < keyASize; i++) {
        keyB[i] = 0xFF ^ keyA[i];
    }
}

static bool microel_read(Nfc* nfc, NfcDevice* device) {
    FURI_LOG_D(TAG, "Entering Microel KDF");

    furi_assert(nfc);
    furi_assert(device);

    bool is_read = false;

    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(device, NfcProtocolMfClassic, data);

    do {
        MfClassicType type = MfClassicType1k;
        MfClassicError error = mf_classic_poller_sync_detect_type(nfc, &type);
        if(error != MfClassicErrorNone) break;

        size_t uid_len;
        const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
        FURI_LOG_D(TAG, "UID identified: %02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
        if(uid_len != UID_LENGTH) break;

        uint8_t keyA[KEY_LENGTH];
        uint8_t keyB[KEY_LENGTH];
        generateKeyA(uid, UID_LENGTH, keyA);
        generateKeyB(keyA, KEY_LENGTH, keyB);

        MfClassicKey key = {0};
        bit_lib_num_to_bytes_be(
            bit_lib_bytes_to_num_be(keyA, KEY_LENGTH), COUNT_OF(key.data), key.data);
        const uint8_t block_num = mf_classic_get_first_block_num_of_sector(0);
        MfClassicAuthContext auth_context;
        error =
            mf_classic_poller_sync_auth(nfc, block_num, &key, MfClassicKeyTypeA, &auth_context);
        if(error != MfClassicErrorNone) {
            break;
        }

        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            if(microel_1k_keys[i].a == 0x000000000000) {
                microel_1k_keys[i].a = bit_lib_bytes_to_num_be(keyA, KEY_LENGTH);
            }
            if(microel_1k_keys[i].b == 0x000000000000) {
                microel_1k_keys[i].b = bit_lib_bytes_to_num_be(keyB, KEY_LENGTH);
            }
        }
        MfClassicDeviceKeys keys = {};
        for(size_t i = 0; i < mf_classic_get_total_sectors_num(data->type); i++) {
            bit_lib_num_to_bytes_be(
                microel_1k_keys[i].a, sizeof(MfClassicKey), keys.key_a[i].data);
            FURI_BIT_SET(keys.key_a_mask, i);
            bit_lib_num_to_bytes_be(
                microel_1k_keys[i].b, sizeof(MfClassicKey), keys.key_b[i].data);
            FURI_BIT_SET(keys.key_b_mask, i);
        }

        error = mf_classic_poller_sync_read(nfc, &keys, data);
        if(error == MfClassicErrorNotPresent) {
            FURI_LOG_W(TAG, "Failed to read data");
            break;
        }

        nfc_device_set_data(device, NfcProtocolMfClassic, data);

        is_read = (error == MfClassicErrorNone);
    } while(false);

    mf_classic_free(data);

    return is_read;
}

static bool microel_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    bool parsed = false;

    do {

        size_t uid_len;
        const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
        if(uid_len != UID_LENGTH) break;

        uint8_t keyA[KEY_LENGTH];
        generateKeyA(uid, UID_LENGTH, keyA);

        MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, verify_sector);
        uint64_t key = bit_lib_bytes_to_num_be(sec_tr->key_a.data, 6);
        uint64_t key_for_check_from_array = bit_lib_bytes_to_num_be(keyA, KEY_LENGTH);
        if(key != key_for_check_from_array) break;

        const uint8_t* temp_ptr = data->block[4].data;
        uint16_t balance = (temp_ptr[6] << 8) | (temp_ptr[5]);
        uint16_t previous_balance = (data->block[5].data[6] << 8) | (data->block[5].data[5]);
        furi_string_cat_printf(parsed_data, "\e#Microel Card\n");
        furi_string_cat_printf(parsed_data, "UID:");
        for(size_t i = 0; i < UID_LENGTH; i++) {
            furi_string_cat_printf(parsed_data, " %02X", uid[i]);
        }
        furi_string_cat_printf(
            parsed_data, "\nCurrent Credit: %d.%02d E \n", balance / 100, balance % 100);
        furi_string_cat_printf(
            parsed_data,
            "Previous Credit: %d.%02d E \n",
            previous_balance / 100,
            previous_balance % 100);

        parsed = true;
    } while(false);

    return parsed;
}

static const NfcSupportedCardsPlugin microel_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify =
        NULL,
    .read = microel_read,
    .parse = microel_parse,
};

static const FlipperAppPluginDescriptor microel_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &microel_plugin,
};

const FlipperAppPluginDescriptor* microel_plugin_ep(void) {
    return &microel_plugin_descriptor;
}
