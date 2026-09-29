#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>

#include "../../api/gallagher/gallagher_util.h"

#include <bit_lib.h>

static bool gallagher_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfClassicData* data = nfc_device_get_data(device, NfcProtocolMfClassic);

    if(!(data->type == MfClassicType1k || data->type == MfClassicType4k)) {
        return false;
    }

    const uint8_t credential_sector_start_block_number =
        mf_classic_get_first_block_num_of_sector(GALLAGHER_CREDENTIAL_SECTOR);

    const uint8_t* credential_block_start_ptr =
        &data->block[credential_sector_start_block_number].data[0];
    uint64_t cardholder_credential = bit_lib_bytes_to_num_be(credential_block_start_ptr, 8);
    uint64_t cardholder_credential_inverse =
        bit_lib_bytes_to_num_be(credential_block_start_ptr + 8, 8);

    if(cardholder_credential != ~cardholder_credential_inverse) {
        return false;
    }

    const uint8_t* cardax_block_start_ptr =
        &data->block[credential_sector_start_block_number + 1].data[0];
    if(memcmp(cardax_block_start_ptr, GALLAGHER_CARDAX_ASCII, MF_CLASSIC_BLOCK_SIZE) != 0) {
        return false;
    }

    GallagherCredential credential;
    gallagher_deobfuscate_and_parse_credential(&credential, credential_block_start_ptr);

    char display_region = 'A';

    if(credential.region < 16) {
        display_region = display_region + (char)credential.region;
    } else {
        display_region = '?';
    }

    furi_string_cat_printf(
        parsed_data,
        "\e#Gallagher NZ\nFacility %c%u\nCard %lu (IL %u)",
        display_region,
        credential.facility,
        credential.card,
        credential.issue);
    return true;
}

static const NfcSupportedCardsPlugin gallagher_plugin = {
    .protocol = NfcProtocolMfClassic,
    .verify = NULL,
    .read = NULL,
    .parse = gallagher_parse,
};

static const FlipperAppPluginDescriptor gallagher_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &gallagher_plugin,
};

const FlipperAppPluginDescriptor* gallagher_plugin_ep(void) {
    return &gallagher_plugin_descriptor;
}
