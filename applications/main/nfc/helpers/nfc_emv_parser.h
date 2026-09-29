#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

bool nfc_emv_parser_get_aid_name(
    Storage* storage,
    const uint8_t* aid,
    uint8_t aid_len,
    FuriString* aid_name);

bool nfc_emv_parser_get_country_name(
    Storage* storage,
    uint16_t country_code,
    FuriString* country_name);

bool nfc_emv_parser_get_currency_name(
    Storage* storage,
    uint16_t currency_code,
    FuriString* currency_name);
