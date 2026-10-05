#pragma once

#include "nfc_device_base_i.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const NfcDeviceBase* const nfc_devices[];

const NfcDeviceBase* nfc_device_get_base(NfcProtocol protocol);

/* FoxFW NFC externalization: firmware-resident protocol name (file-format
 * identifier), available without loading any .fal group. */
const char* nfc_protocol_get_name(NfcProtocol protocol);

#ifdef __cplusplus
}
#endif
