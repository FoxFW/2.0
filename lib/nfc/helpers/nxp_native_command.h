#pragma once

#include "nxp_native_command_mode.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#define NXP_NATIVE_COMMAND_ISO_CLA (0x90)
#define NXP_NATIVE_COMMAND_ISO_P1  (0x00)
#define NXP_NATIVE_COMMAND_ISO_P2  (0x00)
#define NXP_NATIVE_COMMAND_ISO_LE  (0x00)

#define NXP_NATIVE_COMMAND_ISO_SW1 (0x91)

#define NXP_NATIVE_COMMAND_STATUS_OPERATION_OK                (0x00)

#define NXP_NATIVE_COMMAND_STATUS_NO_CHANGES                  (0x0C)

#define NXP_NATIVE_COMMAND_STATUS_OUT_OF_EEPROM_ERROR         (0x0E)

#define NXP_NATIVE_COMMAND_STATUS_ILLEGAL_COMMAND_CODE        (0x1C)

#define NXP_NATIVE_COMMAND_STATUS_INTEGRITY_ERROR             (0x1E)

#define NXP_NATIVE_COMMAND_STATUS_NO_SUCH_KEY                 (0x40)

#define NXP_NATIVE_COMMAND_STATUS_LENGTH_ERROR                (0x7E)

#define NXP_NATIVE_COMMAND_STATUS_PERMISSION_DENIED           (0x9D)

#define NXP_NATIVE_COMMAND_STATUS_PARAMETER_ERROR             (0x9E)

#define NXP_NATIVE_COMMAND_STATUS_APPLICATION_NOT_FOUND       (0xA0)

#define NXP_NATIVE_COMMAND_STATUS_APPL_INTEGRITY_ERROR        (0xA1)

#define NXP_NATIVE_COMMAND_STATUS_STATUS_AUTHENTICATION_DELAY (0xAD)

#define NXP_NATIVE_COMMAND_STATUS_AUTHENTICATION_ERROR        (0xAE)

#define NXP_NATIVE_COMMAND_STATUS_ADDITIONAL_FRAME            (0xAF)

#define NXP_NATIVE_COMMAND_STATUS_BOUNDARY_ERROR              (0xBE)

#define NXP_NATIVE_COMMAND_STATUS_PICC_INTEGRITY_ERROR        (0xC1)

#define NXP_NATIVE_COMMAND_STATUS_COMMAND_ABORTED             (0xCA)

#define NXP_NATIVE_COMMAND_STATUS_PICC_DISABLED_ERROR         (0xCD)

#define NXP_NATIVE_COMMAND_STATUS_COUNT_ERROR                 (0xCE)

#define NXP_NATIVE_COMMAND_STATUS_DUBLICATE_ERROR             (0xDE)

#define NXP_NATIVE_COMMAND_STATUS_EEPROM_ERROR                (0xEE)

#define NXP_NATIVE_COMMAND_STATUS_FILE_NOT_FOUND              (0xF0)

#define NXP_NATIVE_COMMAND_STATUS_FILE_INTEGRITY_ERROR        (0xF1)

typedef uint8_t NxpNativeCommandStatus;

Iso14443_4aError nxp_native_command_iso14443_4a_poller(
    Iso14443_4aPoller* iso14443_4a_poller,
    NxpNativeCommandStatus* status_code,
    const BitBuffer* input_buffer,
    BitBuffer* result_buffer,
    NxpNativeCommandMode command_mode,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer);
