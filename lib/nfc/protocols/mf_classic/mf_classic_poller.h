#pragma once

#include "mf_classic.h"
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfClassicPoller MfClassicPoller;

typedef enum {
    MfClassicPollerEventTypeRequestMode,

    MfClassicPollerEventTypeRequestReadSector,

    MfClassicPollerEventTypeRequestSectorTrailer,
    MfClassicPollerEventTypeRequestWriteBlock,

    MfClassicPollerEventTypeRequestKey,
    MfClassicPollerEventTypeNextSector,
    MfClassicPollerEventTypeDataUpdate,
    MfClassicPollerEventTypeFoundKeyA,
    MfClassicPollerEventTypeFoundKeyB,
    MfClassicPollerEventTypeKeyAttackStart,
    MfClassicPollerEventTypeKeyAttackStop,
    MfClassicPollerEventTypeKeyAttackNextSector,

    MfClassicPollerEventTypeCardDetected,
    MfClassicPollerEventTypeCardLost,
    MfClassicPollerEventTypeSuccess,
    MfClassicPollerEventTypeFail,
} MfClassicPollerEventType;

typedef enum {
    MfClassicPollerModeRead,
    MfClassicPollerModeWrite,
    MfClassicPollerModeDictAttackStandard,
    MfClassicPollerModeDictAttackCUID,
    MfClassicPollerModeDictAttackEnhanced,
} MfClassicPollerMode;

typedef enum {
    MfClassicNestedPhaseNone,
    MfClassicNestedPhaseAnalyzePRNG,
    MfClassicNestedPhaseDictAttack,
    MfClassicNestedPhaseDictAttackVerify,
    MfClassicNestedPhaseDictAttackResume,
    MfClassicNestedPhaseCalibrate,
    MfClassicNestedPhaseRecalibrate,
    MfClassicNestedPhaseCollectNtEnc,
    MfClassicNestedPhaseFinished,
} MfClassicNestedPhase;

typedef enum {
    MfClassicPrngTypeUnknown,
    MfClassicPrngTypeNoTag,
    MfClassicPrngTypeWeak,
    MfClassicPrngTypeHard,
} MfClassicPrngType;

typedef enum {
    MfClassicBackdoorUnknown,
    MfClassicBackdoorNone,
    MfClassicBackdoorAuth1,
    MfClassicBackdoorAuth2,
    MfClassicBackdoorAuth3,
} MfClassicBackdoor;

typedef struct {
    MfClassicPollerMode mode;
    const MfClassicData* data;
} MfClassicPollerEventDataRequestMode;

typedef struct {
    uint8_t current_sector;
} MfClassicPollerEventDataDictAttackNextSector;

typedef struct {
    uint8_t sectors_read;
    uint8_t keys_found;
    uint8_t current_sector;
    MfClassicNestedPhase nested_phase;
    MfClassicPrngType prng_type;
    MfClassicBackdoor backdoor;
    uint16_t nested_target_key;
    uint16_t
        msb_count;
} MfClassicPollerEventDataUpdate;

typedef struct {
    MfClassicKey key;
    MfClassicKeyType key_type;
    bool key_provided;
} MfClassicPollerEventDataKeyRequest;

typedef struct {
    uint8_t sector_num;
    MfClassicKey key;
    MfClassicKeyType key_type;
    bool key_provided;
} MfClassicPollerEventDataReadSectorRequest;

typedef struct {
    uint8_t sector_num;
    MfClassicBlock sector_trailer;
    bool sector_trailer_provided;
} MfClassicPollerEventDataSectorTrailerRequest;

typedef struct {
    uint8_t block_num;
    MfClassicBlock write_block;
    bool write_block_provided;
} MfClassicPollerEventDataWriteBlockRequest;

typedef struct {
    uint8_t current_sector;
} MfClassicPollerEventKeyAttackData;

typedef union {
    MfClassicError error;
    MfClassicPollerEventDataRequestMode poller_mode;
    MfClassicPollerEventDataDictAttackNextSector next_sector_data;
    MfClassicPollerEventDataKeyRequest key_request_data;
    MfClassicPollerEventDataUpdate data_update;
    MfClassicPollerEventDataReadSectorRequest
        read_sector_request_data;
    MfClassicPollerEventKeyAttackData key_attack_data;
    MfClassicPollerEventDataSectorTrailerRequest sec_tr_data;
    MfClassicPollerEventDataWriteBlockRequest write_block_data;
} MfClassicPollerEventData;

typedef struct {
    MfClassicPollerEventType type;
    MfClassicPollerEventData* data;
} MfClassicPollerEvent;

MfClassicError mf_classic_poller_get_nt(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKeyType key_type,
    MfClassicNt* nt,
    bool backdoor_auth);

MfClassicError mf_classic_poller_get_nt_nested(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKeyType key_type,
    MfClassicNt* nt,
    bool backdoor_auth);

MfClassicError mf_classic_poller_auth(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicAuthContext* data,
    bool backdoor_auth);

MfClassicError mf_classic_poller_auth_nested(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicKey* key,
    MfClassicKeyType key_type,
    MfClassicAuthContext* data,
    bool backdoor_auth,
    bool early_ret);

MfClassicError mf_classic_poller_halt(MfClassicPoller* instance);

MfClassicError mf_classic_poller_read_block(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicBlock* data);

MfClassicError mf_classic_poller_write_block(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicBlock* data);

MfClassicError mf_classic_poller_value_cmd(
    MfClassicPoller* instance,
    uint8_t block_num,
    MfClassicValueCommand cmd,
    int32_t data);

MfClassicError mf_classic_poller_value_transfer(MfClassicPoller* instance, uint8_t block_num);

MfClassicError mf_classic_poller_send_standard_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc);

MfClassicError mf_classic_poller_send_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc);

MfClassicError mf_classic_poller_send_custom_parity_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc);

MfClassicError mf_classic_poller_send_encrypted_frame(
    MfClassicPoller* instance,
    const BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    uint32_t fwt_fc);

#ifdef __cplusplus
}
#endif
