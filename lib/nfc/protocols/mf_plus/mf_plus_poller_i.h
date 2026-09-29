#pragma once

#include "mf_plus_poller.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller_i.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MF_PLUS_FWT_FC (60000)

typedef enum {
    MfPlusCardStateDetected,
    MfPlusCardStateLost,
} MfPlusCardState;

typedef enum {
    MfPlusPollerStateIdle,
    MfPlusPollerStateReadVersion,
    MfPlusPollerStateParseVersion,
    MfPlusPollerStateParseIso4,

    MfPlusPollerStateRequestMode,
    MfPlusPollerStateReadSignature,
    MfPlusPollerStateRequestKey,
    MfPlusPollerStateAuthSector,
    MfPlusPollerStateReadSectorBlocks,

    MfPlusPollerStateRequestAdminKey,
    MfPlusPollerStateAuthAdminKey,
    MfPlusPollerStateReadConfig,
    MfPlusPollerStateReadFailed,
    MfPlusPollerStateReadSuccess,

    MfPlusPollerStateRequestWriteSector,
    MfPlusPollerStateAuthWriteSector,
    MfPlusPollerStateRequestWriteBlock,
    MfPlusPollerStateWriteBlock,
    MfPlusPollerStateWriteFailed,
    MfPlusPollerStateWriteSuccess,

    MfPlusPollerStateNum,
} MfPlusPollerState;

typedef struct {
    uint8_t k_enc[MF_PLUS_KEY_SIZE];
    uint8_t k_mac[MF_PLUS_KEY_SIZE];
    uint8_t ti[4];
    uint16_t r_ctr;
    uint16_t w_ctr;
} MfPlusPollerSession;

struct MfPlusPoller {
    Iso14443_4aPoller* iso14443_4a_poller;

    MfPlusData* data;
    MfPlusPollerState state;
    MfPlusPollerSession session;

    MfPlusPollerMode mode;
    MfPlusKey current_key;
    uint8_t current_sector;
    MfPlusKeyType current_key_type;
    uint16_t current_block;
    bool sector_blocks_read;
    uint8_t sectors_read;
    uint8_t keys_found;

    bool read_plain;
    bool read_mode_locked;

    bool write_plain;
    bool write_mode_locked;
    MfPlusBlock current_write_block;

    uint8_t current_admin;
    uint8_t admin_keys_found;
    uint8_t current_config_block;

    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    BitBuffer* input_buffer;
    BitBuffer* result_buffer;

    MfPlusError error;
    NfcGenericEvent general_event;
    MfPlusPollerEvent mfp_event;
    MfPlusPollerEventData mfp_event_data;
    NfcGenericCallback callback;
    void* context;
};

typedef enum {
    MfPlusProbeResultSl0,
    MfPlusProbeResultSl3,
    MfPlusProbeResultNotPlus,
} MfPlusProbeResult;

MfPlusError mf_plus_process_error(Iso14443_4aError error);

MfPlusError mf_plus_poller_probe_security_level(MfPlusPoller* instance, MfPlusProbeResult* result);

MfPlusPoller* mf_plus_poller_alloc(Iso14443_4aPoller* iso14443_4a_poller);

void mf_plus_poller_free(MfPlusPoller* instance);

MfPlusError mf_plus_poller_authenticate_key_id(
    MfPlusPoller* instance,
    uint16_t key_id,
    const MfPlusKey* key,
    MfPlusPollerSession* session);

MfPlusError mf_plus_poller_authenticate(
    MfPlusPoller* instance,
    uint8_t sector,
    MfPlusKeyType key_type,
    const MfPlusKey* key,
    MfPlusPollerSession* session);

MfPlusError mf_plus_poller_read_block(
    MfPlusPoller* instance,
    uint8_t block_low,
    uint8_t block_high,
    bool plain,
    MfPlusPollerSession* session,
    MfPlusBlock* out);

MfPlusError mf_plus_poller_write_block(
    MfPlusPoller* instance,
    uint8_t block_low,
    uint8_t block_high,
    bool plain,
    const MfPlusBlock* in,
    MfPlusPollerSession* session);

MfPlusError
    mf_plus_poller_read_signature(MfPlusPoller* instance, uint8_t* signature, bool* present);

#ifdef __cplusplus
}
#endif
