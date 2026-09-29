#include "nfc_supported_card_plugin.h"
#include <flipper_application.h>

#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#define TAG "AllInOne"

typedef enum {
    AllInOneLayoutTypeA,
    AllInOneLayoutTypeD,
    AllInOneLayoutTypeE2,
    AllInOneLayoutTypeE3,
    AllInOneLayoutTypeE5,
    AllInOneLayoutType2,
    AllInOneLayoutTypeUnknown,
} AllInOneLayoutType;

static AllInOneLayoutType all_in_one_get_layout(const MfUltralightData* data) {

    const uint8_t layout_byte = data->page[5].data[2];
    const uint8_t layout_half_byte = data->page[5].data[2] & 0x0F;

    FURI_LOG_D(TAG, "Layout byte: %02x", layout_byte);
    FURI_LOG_D(TAG, "Layout half-byte: %02x", layout_half_byte);

    switch(layout_half_byte) {

    case 0x0A:
        return AllInOneLayoutTypeA;
    case 0x0D:
        return AllInOneLayoutTypeD;
    case 0x02:
        return AllInOneLayoutType2;
    default:
        FURI_LOG_E(TAG, "Unknown layout type: %d", layout_half_byte);
        return AllInOneLayoutTypeUnknown;
    }
}

static bool all_in_one_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfUltralightData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);

    bool parsed = false;

    do {
        if(data->page[4].data[0] != 0x45 || data->page[4].data[1] != 0xD9) {
            FURI_LOG_E(TAG, "Pass not verified");
            break;
        }

        uint8_t ride_count = 0;
        uint32_t serial = 0;

        const AllInOneLayoutType layout_type = all_in_one_get_layout(data);

        if(layout_type == AllInOneLayoutTypeA) {

            ride_count = data->page[8].data[0];
        } else if(layout_type == AllInOneLayoutTypeD) {

            ride_count = data->page[9].data[1];
        } else {
            FURI_LOG_E(TAG, "Unknown layout: %d", layout_type);
            ride_count = 137;
        }

        const uint8_t* serial_data_lo = data->page[4].data;
        const uint8_t* serial_data_hi = data->page[5].data;

        serial = (serial_data_lo[2] & 0x0F) << 28 | serial_data_lo[3] << 20 |
                 serial_data_hi[0] << 12 | serial_data_hi[1] << 4 | serial_data_hi[2] >> 4;

        furi_string_printf(
            parsed_data, "\e#All-In-One\nNumber: %lu\nRides left: %u", serial, ride_count);

        parsed = true;
    } while(false);

    return parsed;
}

static const NfcSupportedCardsPlugin all_in_one_plugin = {
    .protocol = NfcProtocolMfUltralight,
    .verify = NULL,
    .read = NULL,
    .parse = all_in_one_parse,
};

static const FlipperAppPluginDescriptor all_in_one_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &all_in_one_plugin,
};

const FlipperAppPluginDescriptor* all_in_one_plugin_ep(void) {
    return &all_in_one_plugin_descriptor;
}
