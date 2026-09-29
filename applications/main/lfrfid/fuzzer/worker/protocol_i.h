#pragma once

#include "protocol.h"

#define MAX_PAYLOAD_SIZE (8)
#define PROTOCOL_DEF_IDLE_TIME (4)
#define PROTOCOL_DEF_EMU_TIME (5)
#define PROTOCOL_TIME_DELAY_MIN PROTOCOL_DEF_IDLE_TIME + PROTOCOL_DEF_EMU_TIME

#define PROTOCOL_KEY_FOLDER_NAME "lfrfid"
#define PROTOCOL_KEY_EXTENSION ".rfid"

typedef struct ProtoDict ProtoDict;
typedef struct FuzzerProtocol FuzzerProtocol;

struct ProtoDict {
    const uint8_t* val;
    const uint8_t len;
};

struct FuzzerProtocol {
    const char* name;
    const uint8_t data_size;
    const ProtoDict dict;
};

extern const FuzzerProtocol fuzzer_proto_items[];
