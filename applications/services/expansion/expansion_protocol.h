#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXPANSION_PROTOCOL_DEFAULT_BAUD_RATE (9600UL)

#define EXPANSION_PROTOCOL_MAX_DATA_SIZE (64U)

#define EXPANSION_PROTOCOL_TIMEOUT_MS (250U)

#define EXPANSION_PROTOCOL_BAUD_CHANGE_DT_MS (25U)

typedef enum {
    ExpansionFrameTypeHeartbeat = 1,
    ExpansionFrameTypeStatus = 2,
    ExpansionFrameTypeBaudRate = 3,
    ExpansionFrameTypeControl = 4,
    ExpansionFrameTypeData = 5,
    ExpansionFrameTypeReserved,
} ExpansionFrameType;

typedef enum {
    ExpansionFrameErrorNone = 0x00,
    ExpansionFrameErrorUnknown = 0x01,
    ExpansionFrameErrorBaudRate = 0x02,
} ExpansionFrameError;

typedef enum {

    ExpansionFrameControlCommandStartRpc = 0x00,

    ExpansionFrameControlCommandStopRpc = 0x01,

    ExpansionFrameControlCommandEnableOtg = 0x02,

    ExpansionFrameControlCommandDisableOtg = 0x03,
} ExpansionFrameControlCommand;

#pragma pack(push, 1)

typedef struct {
    uint8_t type;
} ExpansionFrameHeader;

typedef struct {

} ExpansionFrameHeartbeat;

typedef struct {
    uint8_t error;
} ExpansionFrameStatus;

typedef struct {
    uint32_t baud;
} ExpansionFrameBaudRate;

typedef struct {
    uint8_t command;
} ExpansionFrameControl;

typedef struct {

    uint8_t size;

    uint8_t bytes[EXPANSION_PROTOCOL_MAX_DATA_SIZE];
} ExpansionFrameData;

typedef struct {
    ExpansionFrameHeader header;
    union {
        ExpansionFrameHeartbeat heartbeat;
        ExpansionFrameStatus status;
        ExpansionFrameBaudRate baud_rate;
        ExpansionFrameControl control;
        ExpansionFrameData data;
    } content;
} ExpansionFrame;

#pragma pack(pop)

typedef uint8_t ExpansionFrameChecksum;

typedef size_t (*ExpansionFrameReceiveCallback)(uint8_t* data, size_t data_size, void* context);

typedef size_t (*ExpansionFrameSendCallback)(const uint8_t* data, size_t data_size, void* context);

static inline size_t expansion_frame_get_encoded_size(const ExpansionFrame* frame) {
    switch(frame->header.type) {
    case ExpansionFrameTypeHeartbeat:
        return sizeof(frame->header);
    case ExpansionFrameTypeStatus:
        return sizeof(frame->header) + sizeof(frame->content.status);
    case ExpansionFrameTypeBaudRate:
        return sizeof(frame->header) + sizeof(frame->content.baud_rate);
    case ExpansionFrameTypeControl:
        return sizeof(frame->header) + sizeof(frame->content.control);
    case ExpansionFrameTypeData:
        return sizeof(frame->header) + sizeof(frame->content.data.size) + frame->content.data.size;
    default:
        return 0;
    }
}

static inline bool expansion_frame_get_remaining_size(
    const ExpansionFrame* frame,
    size_t received_size,
    size_t* remaining_size) {
    if(received_size < sizeof(ExpansionFrameHeader)) {

        *remaining_size = sizeof(ExpansionFrameHeader);
        return true;
    }

    const size_t received_content_size = received_size - sizeof(ExpansionFrameHeader);
    size_t content_size;

    switch(frame->header.type) {
    case ExpansionFrameTypeHeartbeat:
        content_size = 0;
        break;
    case ExpansionFrameTypeStatus:
        content_size = sizeof(frame->content.status);
        break;
    case ExpansionFrameTypeBaudRate:
        content_size = sizeof(frame->content.baud_rate);
        break;
    case ExpansionFrameTypeControl:
        content_size = sizeof(frame->content.control);
        break;
    case ExpansionFrameTypeData:
        if(received_content_size < sizeof(frame->content.data.size)) {

            content_size = sizeof(frame->content.data.size);
        } else if(frame->content.data.size > sizeof(frame->content.data.bytes)) {

            return false;
        } else {
            content_size = sizeof(frame->content.data.size) + frame->content.data.size;
        }
        break;
    default:
        return false;
    }

    if(content_size > received_content_size) {
        *remaining_size = content_size - received_content_size;
    } else {
        *remaining_size = 0;
    }

    return true;
}

typedef enum {
    ExpansionProtocolStatusOk,
    ExpansionProtocolStatusErrorFormat,
    ExpansionProtocolStatusErrorChecksum,
    ExpansionProtocolStatusErrorCommunication,
} ExpansionProtocolStatus;

static inline ExpansionFrameChecksum
    expansion_protocol_get_checksum(const uint8_t* data, size_t data_size) {
    ExpansionFrameChecksum checksum = 0;
    for(size_t i = 0; i < data_size; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

static inline ExpansionProtocolStatus expansion_protocol_decode(
    ExpansionFrame* frame,
    ExpansionFrameReceiveCallback receive,
    void* context) {
    size_t total_size = 0;
    size_t remaining_size;

    while(true) {
        if(!expansion_frame_get_remaining_size(frame, total_size, &remaining_size)) {
            return ExpansionProtocolStatusErrorFormat;
        } else if(remaining_size == 0) {
            break;
        }

        const size_t received_size =
            receive((uint8_t*)frame + total_size, remaining_size, context);

        if(received_size == 0) {
            return ExpansionProtocolStatusErrorCommunication;
        }

        total_size += received_size;
    }

    ExpansionFrameChecksum checksum;
    const size_t received_size = receive(&checksum, sizeof(checksum), context);

    if(received_size != sizeof(checksum)) {
        return ExpansionProtocolStatusErrorCommunication;
    } else if(checksum != expansion_protocol_get_checksum((const uint8_t*)frame, total_size)) {
        return ExpansionProtocolStatusErrorChecksum;
    } else {
        return ExpansionProtocolStatusOk;
    }
}

static inline ExpansionProtocolStatus expansion_protocol_encode(
    const ExpansionFrame* frame,
    ExpansionFrameSendCallback send,
    void* context) {
    const size_t encoded_size = expansion_frame_get_encoded_size(frame);
    if(encoded_size == 0) {
        return ExpansionProtocolStatusErrorFormat;
    }

    const ExpansionFrameChecksum checksum =
        expansion_protocol_get_checksum((const uint8_t*)frame, encoded_size);

    if((send((const uint8_t*)frame, encoded_size, context) != encoded_size) ||
       (send(&checksum, sizeof(checksum), context) != sizeof(checksum))) {
        return ExpansionProtocolStatusErrorCommunication;
    } else {
        return ExpansionProtocolStatusOk;
    }
}

#ifdef __cplusplus
}
#endif
