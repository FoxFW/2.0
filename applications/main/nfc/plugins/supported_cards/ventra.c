#include "nfc_supported_card_plugin.h"

#include <flipper_application/flipper_application.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include "datetime.h"
#include <furi_hal.h>

#define TAG "Ventra"

DateTime ventra_exp_date = {0}, ventra_validity_date = {0};
uint8_t ventra_high_seq = 0, ventra_cur_blk = 0, ventra_mins_active = 0;

uint32_t time_now() {
    return furi_hal_rtc_get_timestamp();
}

static DateTime dt_delta(DateTime dt, uint8_t delta_days) {

    DateTime dt_shifted = {0};
    datetime_timestamp_to_datetime(
        datetime_datetime_to_timestamp(&dt) - (uint64_t)delta_days * 86400, &dt_shifted);
    return dt_shifted;
}

bool isExpired(void) {
    uint32_t ts_hard_exp = datetime_datetime_to_timestamp(&ventra_exp_date);
    uint32_t ts_soft_exp = datetime_datetime_to_timestamp(&ventra_validity_date);
    uint32_t ts_now = time_now();
    return (ts_now >= ts_hard_exp || ts_now > ts_soft_exp);
}

static FuriString* ventra_parse_xact(const MfUltralightData* data, uint8_t blk, bool is_pass) {
    FuriString* ventra_xact_str = furi_string_alloc();
    uint16_t ts = data->page[blk].data[0] | data->page[blk].data[1] << 8;
    uint8_t tran_type = ts & 0x1F;
    ts >>= 5;
    uint8_t day = data->page[blk].data[2];
    uint32_t work = data->page[blk + 1].data[0] | data->page[blk + 1].data[1] << 8 |
                    data->page[blk + 1].data[2] << 16;
    uint8_t seq = work & 0x7F;
    uint16_t exp = (work >> 7) & 0x7FF;
    uint8_t exp_day = data->page[blk + 2].data[0];
    uint16_t locus = data->page[blk + 2].data[1] | data->page[blk + 2].data[2] << 8;
    uint8_t line = data->page[blk + 2].data[3];

    DateTime dt = dt_delta(ventra_exp_date, day);
    dt.hour = (ts & 0x7FF) / 60;
    dt.minute = (ts & 0x7FF) % 60;

    if(seq == 0) {
        furi_string_printf(ventra_xact_str, "-- EMPTY --");
        return (ventra_xact_str);
    }
    if(seq > ventra_high_seq) {
        ventra_high_seq = seq;
        ventra_cur_blk = blk;
        ventra_mins_active = data->page[blk + 1].data[3];

        if(tran_type == 6) {
            if(is_pass) {
                ventra_validity_date = dt_delta(ventra_exp_date, exp_day);
                ventra_validity_date.hour = (exp & 0x7FF) / 60;
                ventra_validity_date.minute = (exp & 0x7FF) % 60;
            } else {
                uint32_t validity_ts = datetime_datetime_to_timestamp(&dt);
                validity_ts += (120 - ventra_mins_active) * 60;
                datetime_timestamp_to_datetime(validity_ts, &ventra_validity_date);
            }
        }
    }

    char linemap[3] = "PTB";
    char* xact_fmt = (line == 2) ? "%c %5d %04d-%02d-%02d %02d:%02d" :
                                   "%c %04X %04d-%02d-%02d %02d:%02d";

    furi_string_printf(
        ventra_xact_str,
        xact_fmt,
        (line < 3) ? linemap[line] : '?',
        locus,
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute);
    return (ventra_xact_str);
}

static bool ventra_parse(const NfcDevice* device, FuriString* parsed_data) {
    furi_assert(device);
    furi_assert(parsed_data);

    const MfUltralightData* data = nfc_device_get_data(device, NfcProtocolMfUltralight);

    bool parsed = false;

    do {

        if(data->page[4].data[0] != 0x0A || data->page[4].data[1] != 4 ||
           data->page[4].data[2] != 0 || data->page[6].data[0] != 0 ||
           data->page[6].data[1] != 0 || data->page[6].data[2] != 0) {
            FURI_LOG_D(TAG, "Not Ventra Ultralight");
            break;
        }

        FuriString* ventra_prod_str = furi_string_alloc();
        uint8_t otp = data->page[3].data[0];
        uint8_t prod_code = data->page[5].data[2];
        bool is_pass = false;
        switch(prod_code) {
        case 2:
        case 0x1F:
            furi_string_cat_printf(ventra_prod_str, "Single");
            break;
        case 3:
        case 0x3F:
            is_pass = true;
            furi_string_cat_printf(ventra_prod_str, "1-Day");
            break;
        case 4:
            is_pass = true;
            furi_string_cat_printf(ventra_prod_str, "3-Day");
            break;
        default:
            is_pass =
                true;
            furi_string_cat_printf(ventra_prod_str, "0x%02X", data->page[5].data[2]);
            break;
        }

        uint16_t date_y = data->page[4].data[3] | (data->page[5].data[0] << 8);
        uint8_t date_d = date_y & 0x1F;
        uint8_t date_m = (date_y >> 5) & 0x0F;
        date_y >>= 9;
        date_y += 2000;
        ventra_exp_date.day = date_d;
        ventra_exp_date.month = date_m;
        ventra_exp_date.year = date_y;
        ventra_validity_date = ventra_exp_date;

        FuriString* ventra_xact_str1 = ventra_parse_xact(data, 8, is_pass);
        FuriString* ventra_xact_str2 = ventra_parse_xact(data, 12, is_pass);

        uint8_t card_state = 1;
        uint8_t rides_left = 0;

        char* card_states[5] = {"???", "NEW", "ACT", "USED", "EXP"};

        if(ventra_high_seq > 1) card_state = 2;

        if(!is_pass) {
            switch(otp) {
            case 0:
                rides_left = 3;
                break;
            case 2:
                card_state = 2;
                rides_left = 2;
                break;
            case 6:
                card_state = 2;
                rides_left = 1;
                break;
            case 0x0E:
            case 0x7E:
                card_state = 3;
                rides_left = 0;
                break;
            default:
                card_state = 0;
                rides_left = 0;
                break;
            }
        }
        if(isExpired()) {
            card_state = 4;
            rides_left = 0;
        }

        furi_string_printf(
            parsed_data,
            "\e#Ventra %s (%s)\n",
            furi_string_get_cstr(ventra_prod_str),
            card_states[card_state]);

        furi_string_cat_printf(
            parsed_data,
            "Exp: %04d-%02d-%02d %02d:%02d\n",
            ventra_validity_date.year,
            ventra_validity_date.month,
            ventra_validity_date.day,
            ventra_validity_date.hour,
            ventra_validity_date.minute);

        if(rides_left) {
            furi_string_cat_printf(parsed_data, "Rides left: %d\n", rides_left);
        }

        furi_string_cat_printf(
            parsed_data,
            "%s\n",
            furi_string_get_cstr(ventra_cur_blk == 8 ? ventra_xact_str1 : ventra_xact_str2));

        furi_string_cat_printf(
            parsed_data,
            "%s\n",
            furi_string_get_cstr(ventra_cur_blk == 8 ? ventra_xact_str2 : ventra_xact_str1));

        furi_string_cat_printf(
            parsed_data, "TVM ID: %02X%02X\n", data->page[7].data[1], data->page[7].data[0]);
        furi_string_cat_printf(parsed_data, "Tx count: %d\n", ventra_high_seq);
        furi_string_cat_printf(
            parsed_data,
            "Hard Expiry: %04d-%02d-%02d",
            ventra_exp_date.year,
            ventra_exp_date.month,
            ventra_exp_date.day);

        furi_string_free(ventra_prod_str);
        furi_string_free(ventra_xact_str1);
        furi_string_free(ventra_xact_str2);

        parsed = true;
    } while(false);

    return parsed;
}

static const NfcSupportedCardsPlugin ventra_plugin = {
    .protocol = NfcProtocolMfUltralight,
    .verify = NULL,
    .read = NULL,
    .parse = ventra_parse,
};

static const FlipperAppPluginDescriptor ventra_plugin_descriptor = {
    .appid = NFC_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = NFC_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &ventra_plugin,
};

const FlipperAppPluginDescriptor* ventra_plugin_ep(void) {
    return &ventra_plugin_descriptor;
}
