#pragma once

#include <nfc/protocols/mf_plus/mf_plus.h>

#include "../nfc_protocol_support_render_common.h"

void nfc_render_mf_plus_info(
    const MfPlusData* data,
    NfcProtocolFormatType format_type,
    FuriString* str);

void nfc_render_mf_plus_dump(const MfPlusData* data, FuriString* str);

void nfc_render_mf_plus_iso14443_4(const MfPlusData* data, FuriString* str);

void nfc_render_mf_plus_version_info(const MfPlusData* data, FuriString* str);

void nfc_render_mf_plus_version(const MfPlusVersion* data, FuriString* str);
