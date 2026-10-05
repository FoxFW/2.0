#pragma once

#include "mf_plus.h"

#include <nfc/helpers/nxp_native_command.h>

#define MF_PLUS_FFF_PICC_PREFIX "PICC"

MfPlusError mf_plus_get_type_from_version(
    const Iso14443_4aData* iso14443_4a_data,
    MfPlusSecurityLevel probed_security_level,
    MfPlusData* mf_plus_data);

MfPlusError mf_plus_get_type_from_iso4(
    const Iso14443_4aData* iso4_data,
    MfPlusSecurityLevel probed_security_level,
    MfPlusData* mf_plus_data);

MfPlusError mf_plus_version_parse(MfPlusVersion* data, const BitBuffer* buf);

bool mf_plus_version_load(MfPlusVersion* data, FlipperFormat* ff);

bool mf_plus_security_level_load(MfPlusSecurityLevel* data, FlipperFormat* ff);

bool mf_plus_type_load(MfPlusType* data, FlipperFormat* ff);

bool mf_plus_size_load(MfPlusSize* data, FlipperFormat* ff);

bool mf_plus_version_save(const MfPlusVersion* data, FlipperFormat* ff);

bool mf_plus_security_level_save(const MfPlusSecurityLevel* data, FlipperFormat* ff);

bool mf_plus_type_save(const MfPlusType* data, FlipperFormat* ff);

bool mf_plus_size_save(const MfPlusSize* data, FlipperFormat* ff);

void mf_plus_set_block_read(MfPlusData* data, uint16_t block_num, const MfPlusBlock* block);

void mf_plus_set_key_found(
    MfPlusData* data,
    uint8_t sector,
    MfPlusKeyType key_type,
    const MfPlusKey* key);

void mf_plus_set_admin_key_found(MfPlusData* data, MfPlusAdminKeyType type, const MfPlusKey* key);

void mf_plus_set_config_block_read(MfPlusData* data, uint8_t index, const MfPlusBlock* block);

uint16_t mf_plus_get_admin_key_address(MfPlusAdminKeyType type);

bool mf_plus_admin_key_type_from_address(uint16_t address, MfPlusAdminKeyType* type);

bool mf_plus_sl3_data_save(const MfPlusData* data, FlipperFormat* ff);

bool mf_plus_sl3_data_load(MfPlusData* data, FlipperFormat* ff);
