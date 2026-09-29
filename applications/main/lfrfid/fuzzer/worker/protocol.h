#pragma once

#include <stdint.h>

typedef struct FuzzerPayload FuzzerPayload;

typedef uint8_t FuzzerProtocolsID;

typedef enum {
    FuzzerAttackIdDefaultValues = 0,
    FuzzerAttackIdLoadFile,
    FuzzerAttackIdLoadFileCustomUids,
    FuzzerAttackIdBFCustomerID,
} FuzzerAttackId;

struct FuzzerPayload {
    uint8_t* data;
    uint8_t data_size;
};

FuzzerPayload* fuzzer_payload_alloc();

void fuzzer_payload_free(FuzzerPayload*);

uint8_t fuzzer_proto_get_max_data_size();

uint8_t fuzzer_proto_get_def_emu_time();

uint8_t fuzzer_proto_get_def_idle_time();

const char* fuzzer_proto_get_name(FuzzerProtocolsID index);

uint8_t fuzzer_proto_get_count_of_protocols();

const char* fuzzer_proto_get_menu_label(uint8_t index);

FuzzerAttackId fuzzer_proto_get_attack_id_by_index(uint8_t index);

uint8_t fuzzer_proto_get_count_of_menu_items();
