#pragma once

#include "mf_plus.h"

#include <lib/nfc/protocols/iso14443_4a/iso14443_4a.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MfPlusPoller MfPlusPoller;

typedef enum {
    MfPlusPollerEventTypeRequestMode,
    MfPlusPollerEventTypeRequestKey,
    MfPlusPollerEventTypeDataUpdate,
    MfPlusPollerEventTypeReadSuccess,
    MfPlusPollerEventTypeReadFailed,
    MfPlusPollerEventTypeRequestWriteSector,
    MfPlusPollerEventTypeRequestWriteBlock,
    MfPlusPollerEventTypeWriteSuccess,
    MfPlusPollerEventTypeWriteFailed,
} MfPlusPollerEventType;

typedef enum {
    MfPlusPollerModeInfo,
    MfPlusPollerModeRead,
    MfPlusPollerModeWrite,
} MfPlusPollerMode;

typedef struct {
    MfPlusPollerMode mode;
} MfPlusPollerEventDataRequestMode;

typedef struct {
    bool is_admin;
    uint8_t sector;
    MfPlusKeyType key_type;
    MfPlusAdminKeyType admin_type;
    MfPlusKey key;
    bool key_provided;
} MfPlusPollerEventDataKeyRequest;

typedef struct {
    uint8_t current_sector;
    uint8_t sectors_read;
    uint8_t keys_found;
} MfPlusPollerEventDataUpdate;

typedef struct {
    uint8_t sector;
    MfPlusKeyType key_type;
    MfPlusKey key;
    bool key_provided;
} MfPlusPollerEventDataWriteSectorRequest;

typedef struct {
    uint16_t block_num;
    MfPlusBlock block;
    bool block_provided;
} MfPlusPollerEventDataWriteBlockRequest;

typedef union {
    MfPlusError error;
    MfPlusPollerEventDataRequestMode mode_request;
    MfPlusPollerEventDataKeyRequest key_request;
    MfPlusPollerEventDataUpdate data_update;
    MfPlusPollerEventDataWriteSectorRequest write_sector_request;
    MfPlusPollerEventDataWriteBlockRequest write_block_request;
} MfPlusPollerEventData;

typedef struct {
    MfPlusPollerEventType type;
    MfPlusPollerEventData* data;
} MfPlusPollerEvent;

MfPlusError mf_plus_poller_read_version(MfPlusPoller* instance, MfPlusVersion* data);

#ifdef __cplusplus
}
#endif
