#pragma once

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_PLUS_UID_SIZE_MAX (7)
#define MF_PLUS_BATCH_SIZE   (5)

#define MF_PLUS_CMD_GET_VERSION (0x60)

typedef enum {
    MfPlusErrorNone,
    MfPlusErrorUnknown,
    MfPlusErrorNotPresent,
    MfPlusErrorProtocol,
    MfPlusErrorAuth,
    MfPlusErrorPartialRead,
    MfPlusErrorTimeout,

    MfPlusErrorRejected,
} MfPlusError;

typedef enum {
    MfPlusTypePlus,
    MfPlusTypeEV1,
    MfPlusTypeEV2,
    MfPlusTypeS,
    MfPlusTypeSE,
    MfPlusTypeX,

    MfPlusTypeUnknown,
    MfPlusTypeNum,
} MfPlusType;

typedef enum {
    MfPlusSize1K,
    MfPlusSize2K,
    MfPlusSize4K,

    MfPlusSizeUnknown,
    MfPlusSizeNum,
} MfPlusSize;

typedef enum {
    MfPlusSecurityLevel0,
    MfPlusSecurityLevel1,
    MfPlusSecurityLevel2,
    MfPlusSecurityLevel3,

    MfPlusSecurityLevelUnknown,
    MfPlusSecurityLevelNum,
} MfPlusSecurityLevel;

typedef struct {
    uint8_t hw_vendor;
    uint8_t hw_type;
    uint8_t hw_subtype;
    uint8_t hw_major;
    uint8_t hw_minor;
    uint8_t hw_storage;
    uint8_t hw_proto;

    uint8_t sw_vendor;
    uint8_t sw_type;
    uint8_t sw_subtype;
    uint8_t sw_major;
    uint8_t sw_minor;
    uint8_t sw_storage;
    uint8_t sw_proto;

    uint8_t uid[MF_PLUS_UID_SIZE_MAX];
    uint8_t batch[MF_PLUS_BATCH_SIZE];
    uint8_t prod_week;
    uint8_t prod_year;
} MfPlusVersion;

#define MF_PLUS_BLOCK_SIZE     (16)
#define MF_PLUS_KEY_SIZE       (16)
#define MF_PLUS_SIGNATURE_SIZE (56)

#define MF_PLUS_MAX_SECTORS          (40)
#define MF_PLUS_MAX_BLOCKS           (256)
#define MF_PLUS_BLOCK_READ_MASK_SIZE (MF_PLUS_MAX_BLOCKS / 32)
#define MF_PLUS_CONFIG_BLOCK_NUM     (4)

typedef struct {
    uint8_t data[MF_PLUS_BLOCK_SIZE];
} MfPlusBlock;

typedef struct {
    uint8_t data[MF_PLUS_KEY_SIZE];
} MfPlusKey;

typedef enum {
    MfPlusKeyTypeA = 0,
    MfPlusKeyTypeB = 1,
} MfPlusKeyType;

typedef enum {
    MfPlusAdminKeyCardMaster,
    MfPlusAdminKeyCardConfig,
    MfPlusAdminKeyL3Switch,
    MfPlusAdminKeySL1CardAuth,

    MfPlusAdminKeyNum,
} MfPlusAdminKeyType;

typedef struct {
    Iso14443_4aData* iso14443_4a_data;
    MfPlusVersion version;
    MfPlusType type;
    MfPlusSize size;
    MfPlusSecurityLevel security_level;
    FuriString* device_name;

    bool signature_present;
    uint8_t signature[MF_PLUS_SIGNATURE_SIZE];

    MfPlusBlock block[MF_PLUS_MAX_BLOCKS];
    uint32_t block_read_mask[MF_PLUS_BLOCK_READ_MASK_SIZE];

    MfPlusKey key_a[MF_PLUS_MAX_SECTORS];
    MfPlusKey key_b[MF_PLUS_MAX_SECTORS];
    uint64_t key_a_mask;
    uint64_t key_b_mask;

    MfPlusKey admin_key[MfPlusAdminKeyNum];
    uint8_t admin_key_mask;
    MfPlusBlock config_block[MF_PLUS_CONFIG_BLOCK_NUM];
    uint8_t config_read_mask;
} MfPlusData;

extern const NfcDeviceBase nfc_device_mf_plus;

uint8_t mf_plus_get_sector_count(MfPlusSize size);

bool mf_plus_is_card_read(const MfPlusData* data);

void mf_plus_get_read_sectors_and_keys(
    const MfPlusData* data,
    uint8_t* sectors_read,
    uint8_t* keys_found);

uint16_t mf_plus_get_block_count(MfPlusSize size);

uint16_t mf_plus_sector_get_first_block(uint8_t sector);

uint8_t mf_plus_sector_get_block_count(uint8_t sector);

bool mf_plus_is_block_read(const MfPlusData* data, uint16_t block_num);

bool mf_plus_is_key_found(const MfPlusData* data, uint8_t sector, MfPlusKeyType key_type);

bool mf_plus_is_admin_key_found(const MfPlusData* data, MfPlusAdminKeyType type);

bool mf_plus_is_config_block_read(const MfPlusData* data, uint8_t index);

const char* mf_plus_get_admin_key_name(MfPlusAdminKeyType type);

MfPlusData* mf_plus_alloc(void);

void mf_plus_free(MfPlusData* data);

void mf_plus_reset(MfPlusData* data);

void mf_plus_copy(MfPlusData* data, const MfPlusData* other);

void mf_plus_merge_update(MfPlusData* base, const MfPlusData* fresh);

bool mf_plus_verify(MfPlusData* data, const FuriString* device_type);

bool mf_plus_load(MfPlusData* data, FlipperFormat* ff, uint32_t version);

bool mf_plus_save(const MfPlusData* data, FlipperFormat* ff);

bool mf_plus_is_equal(const MfPlusData* data, const MfPlusData* other);

const char* mf_plus_get_device_name(const MfPlusData* data, NfcDeviceNameType name_type);

const uint8_t* mf_plus_get_uid(const MfPlusData* data, size_t* uid_len);

bool mf_plus_set_uid(MfPlusData* data, const uint8_t* uid, size_t uid_len);

Iso14443_4aData* mf_plus_get_base_data(const MfPlusData* data);

#ifdef __cplusplus
}
#endif
