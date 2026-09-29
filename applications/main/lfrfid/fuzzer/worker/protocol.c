#include "protocol_i.h"
#include "furi.h"

#define THREEBYTE_DATA_SIZE (3)
#define FOURBYTE_DATA_SIZE (4)
#define FIVEBYTE_DATA_SIZE (5)
#define SIXBYTE_DATA_SIZE (6)
#define EIGHTBYTE_DATA_SIZE (8)

const uint8_t uid_list_5byte[][FIVEBYTE_DATA_SIZE] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x11, 0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44, 0x44},
    {0x55, 0x55, 0x55, 0x55, 0x55},
    {0x66, 0x66, 0x66, 0x66, 0x66},
    {0x77, 0x77, 0x77, 0x77, 0x77},
    {0x88, 0x88, 0x88, 0x88, 0x88},
    {0x99, 0x99, 0x99, 0x99, 0x99},
    {0x12, 0x34, 0x56, 0x78, 0x9A},
    {0x9A, 0x78, 0x56, 0x34, 0x12},
    {0x04, 0xd0, 0x9b, 0x0d, 0x6a},
    {0x34, 0x00, 0x29, 0x3d, 0x9e},
    {0x04, 0xdf, 0x00, 0x00, 0x01},
    {0xCA, 0xCA, 0xCA, 0xCA, 0xCA},
};

const uint8_t uid_list_6byte[][SIXBYTE_DATA_SIZE] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
    {0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66},
    {0x77, 0x77, 0x77, 0x77, 0x77, 0x77},
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88},
    {0x99, 0x99, 0x99, 0x99, 0x99, 0x99},
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},
    {0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12},
    {0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA},
};

const uint8_t uid_list_4byte[][FOURBYTE_DATA_SIZE] = {
    {0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF},
    {0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44},
    {0x55, 0x55, 0x55, 0x55},
    {0x66, 0x66, 0x66, 0x66},
    {0x77, 0x77, 0x77, 0x77},
    {0x88, 0x88, 0x88, 0x88},
    {0x99, 0x99, 0x99, 0x99},
    {0x12, 0x34, 0x56, 0x78},
    {0x9A, 0x78, 0x56, 0x34},
    {0x04, 0xd0, 0x9b, 0x0d},
    {0x34, 0x00, 0x29, 0x3d},
    {0x04, 0xdf, 0x00, 0x00},
    {0xCA, 0xCA, 0xCA, 0xCA},
};

const uint8_t uid_list_3byte[][THREEBYTE_DATA_SIZE] = {
    {0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF},
    {0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44},
    {0x55, 0x55, 0x55},
    {0x66, 0x66, 0x66},
    {0x77, 0x77, 0x77},
    {0x88, 0x88, 0x88},
    {0x99, 0x99, 0x99},
    {0x12, 0x34, 0x56},
    {0x56, 0x34, 0x12},
    {0xCA, 0xCA, 0xCA},
};

const uint8_t uid_list_8byte[][EIGHTBYTE_DATA_SIZE] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
    {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33},
    {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
    {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55},
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66},
    {0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77},
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88},
    {0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99},
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFF},
    {0xFF, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12},
    {0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA, 0xCA},
};

const FuzzerProtocol fuzzer_proto_items[] = {

    {
        .name = "EM4100",
        .data_size = FIVEBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_5byte,
                .len = COUNT_OF(uid_list_5byte),
            },
    },

    {
        .name = "HIDProx",
        .data_size = SIXBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_6byte,
                .len = COUNT_OF(uid_list_6byte),
            },
    },

    {
        .name = "PAC/Stanley",
        .data_size = FOURBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_4byte,
                .len = COUNT_OF(uid_list_4byte),
            },
    },

    {
        .name = "H10301",
        .data_size = THREEBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_3byte,
                .len = COUNT_OF(uid_list_3byte),
            },
    },

    {
        .name = "IoProxXSF",
        .data_size = FOURBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_4byte,
                .len = COUNT_OF(uid_list_4byte),
            },
    },

    {
        .name = "Paradox",
        .data_size = SIXBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_6byte,
                .len = COUNT_OF(uid_list_6byte),
            },
    },

    {
        .name = "Indala26",
        .data_size = FOURBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_4byte,
                .len = COUNT_OF(uid_list_4byte),
            },
    },

    {
        .name = "Viking",
        .data_size = FOURBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_4byte,
                .len = COUNT_OF(uid_list_4byte),
            },
    },

    {
        .name = "Pyramid",
        .data_size = FOURBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_4byte,
                .len = COUNT_OF(uid_list_4byte),
            },
    },

    {
        .name = "Keri",
        .data_size = FOURBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_4byte,
                .len = COUNT_OF(uid_list_4byte),
            },
    },

    {
        .name = "Jablotron",
        .data_size = FIVEBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_5byte,
                .len = COUNT_OF(uid_list_5byte),
            },
    },

    {
        .name = "Electra",
        .data_size = EIGHTBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_8byte,
                .len = COUNT_OF(uid_list_8byte),
            },
    },

    {
        .name = "Idteck",
        .data_size = EIGHTBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_8byte,
                .len = COUNT_OF(uid_list_8byte),
            },
    },

    {
        .name = "Gallagher",
        .data_size = EIGHTBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_8byte,
                .len = COUNT_OF(uid_list_8byte),
            },
    },

    {
        .name = "Nexwatch",
        .data_size = EIGHTBYTE_DATA_SIZE,
        .dict =
            {
                .val = (const uint8_t*)&uid_list_8byte,
                .len = COUNT_OF(uid_list_8byte),
            },
    },
};

typedef struct {
    const char* menu_label;
    FuzzerAttackId attack_id;
} FuzzerMenuItems;

const FuzzerMenuItems fuzzer_menu_items[] = {
    {"Default Values", FuzzerAttackIdDefaultValues},
    {"BF Customer ID", FuzzerAttackIdBFCustomerID},
    {"Load File", FuzzerAttackIdLoadFile},
    {"Load UIDs from file", FuzzerAttackIdLoadFileCustomUids},
};

FuzzerPayload* fuzzer_payload_alloc() {
    FuzzerPayload* payload = malloc(sizeof(FuzzerPayload));
    payload->data = malloc(sizeof(payload->data[0]) * MAX_PAYLOAD_SIZE);

    return payload;
}

void fuzzer_payload_free(FuzzerPayload* payload) {
    furi_assert(payload);

    if(payload->data) {
        free(payload->data);
    }
    free(payload);
}

const char* fuzzer_proto_get_name(FuzzerProtocolsID index) {
    return fuzzer_proto_items[index].name;
}

uint8_t fuzzer_proto_get_count_of_protocols() {
    return COUNT_OF(fuzzer_proto_items);
}

uint8_t fuzzer_proto_get_max_data_size() {
    return MAX_PAYLOAD_SIZE;
}

uint8_t fuzzer_proto_get_def_emu_time() {
    return PROTOCOL_DEF_EMU_TIME;
}

uint8_t fuzzer_proto_get_def_idle_time() {
    return PROTOCOL_DEF_IDLE_TIME;
}

const char* fuzzer_proto_get_menu_label(uint8_t index) {
    return fuzzer_menu_items[index].menu_label;
}

FuzzerAttackId fuzzer_proto_get_attack_id_by_index(uint8_t index) {
    return fuzzer_menu_items[index].attack_id;
}

uint8_t fuzzer_proto_get_count_of_menu_items() {
    return COUNT_OF(fuzzer_menu_items);
}
