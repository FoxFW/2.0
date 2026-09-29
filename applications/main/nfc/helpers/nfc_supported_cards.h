#pragma once

#include <core/string.h>
#include <flipper_application/plugins/composite_resolver.h>

#include <nfc/nfc.h>
#include <nfc/nfc_device.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcSupportedCards NfcSupportedCards;

NfcSupportedCards* nfc_supported_cards_alloc(CompositeApiResolver* api_resolver);

void nfc_supported_cards_free(NfcSupportedCards* instance);

void nfc_supported_cards_load_cache(NfcSupportedCards* instance);

bool nfc_supported_cards_read(NfcSupportedCards* instance, NfcDevice* device, Nfc* nfc);

bool nfc_supported_cards_parse(
    NfcSupportedCards* instance,
    NfcDevice* device,
    FuriString* parsed_data);

#ifdef __cplusplus
}
#endif
